// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "server.h"
#include <cassert>

using namespace netcode;

NCauthority::NCauthority(const NCprotocol * protocol) : protocol(protocol), frame()
{

}

NCauthority::~NCauthority()
{
    // If there are any outstanding peers, remove any references they have to objects or to the authority
    for(auto peer : peers)
    {
        peer->visChanges.clear();
        peer->records.clear();
        peer->auth = nullptr;
    }

    // If there are any outstanding objects, remove their reference to the authority
    for(auto object : objects) object->auth = nullptr;
}

NCpeer * NCauthority::CreatePeer()
{
	auto peer = new NCpeer(this);
	peers.push_back(peer);
	return peer;    
}

NCobject * NCauthority::CreateObject(const NCclass * cl)
{
    if(cl->protocol != protocol) return nullptr;

	auto object = new NCobject(this, cl, stateAlloc.Allocate(cl->sizeInBytes));
    if(stateAlloc.GetTotalCapacity() > state.size()) state.resize(stateAlloc.GetTotalCapacity(), 0);
	objects.push_back(object);
	return object;
}

void NCauthority::PublishFrame()
{
    ++frame;
    frameState[frame] = state;

    int oldestAck = INT_MAX;
    for(auto peer : peers)
    {
        peer->OnPublishFrame(frame);
        oldestAck = std::min(oldestAck, peer->GetOldestAckFrame());
    }

    // Once all clients have acknowledged a certain frame, expire all older frames
    frameState.erase(begin(frameState), frameState.lower_bound(std::min(frame - protocol->maxFrameDelta, oldestAck)));
}

//////////////
// NCobject //
//////////////

NCobject::NCobject(NCauthority * auth, const NCclass * cl, int stateOffset) : auth(auth), cl(cl), stateOffset(stateOffset)
{
    
}

NCobject::~NCobject()
{
    if(auth)
    {
        auth->stateAlloc.Free(stateOffset, cl->sizeInBytes);
        for(auto peer : auth->peers) peer->SetVisibility(this, false);
        auto it = std::find(begin(auth->objects), end(auth->objects), this);
        if(it != end(auth->objects)) auth->objects.erase(it);
    }
}

void NCobject::SetIntField(const NCint * field, int value)
{ 
    if(!auth || field->cl != cl) return;
    reinterpret_cast<int &>(auth->state[stateOffset + field->dataOffset]) = value; 
}

////////////
// Client //
////////////

struct Frameset
{
    int32_t frame, prevFrames[4];
    const uint8_t * prevStates[4];
    CurvePredictor predictors[5];

    void RefreshPredictors()
    {
        predictors[0] = CurvePredictor();
        predictors[1] = prevFrames[0] != 0 ? MakeConstantPredictor() : predictors[0];
        predictors[2] = prevFrames[1] != 0 ? MakeLinearPredictor(frame-prevFrames[0], frame-prevFrames[1]) : predictors[1];
        predictors[3] = prevFrames[2] != 0 ? MakeQuadraticPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2]) : predictors[1];
        predictors[4] = prevFrames[3] != 0 ? MakeCubicPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2], frame-prevFrames[3]) : predictors[1];
    }

    void EncodeFrameList(ArithmeticEncoder & encoder, const NCprotocol & protocol)
    {
        EncodeBits(encoder, frame, 32);
        for(int i=0; i<4; ++i)
        {
            code_t value = prevFrames[i] ? frame - prevFrames[i] : 0;
            EncodeUniform(encoder, value, protocol.maxFrameDelta+1);
        }
    }

    void DecodeFrameList(ArithmeticDecoder & decoder, const NCprotocol & protocol)
    {
        frame = DecodeBits(decoder, 32);
        for(int i=0; i<4; ++i)
        {
            code_t value = DecodeUniform(decoder, protocol.maxFrameDelta+1);
            prevFrames[i] = value ? frame - value : 0;
        }
        RefreshPredictors();
    }

    int GetSampleCount(int frameAdded) const { for(int i=4; i>0; --i) if(frameAdded <= prevFrames[i-1]) return i; return 0; }

    void EncodeAndTallyObject(ArithmeticEncoder & encoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, const uint8_t * state)
    {
        const int sampleCount = GetSampleCount(frameAdded);
        for(auto field : cl.fields)
		{
            int offset = stateOffset + field->dataOffset, prevValues[4];
            for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
		    distribs.intFieldDists[field->uniqueId].EncodeAndTally(encoder, reinterpret_cast<const int &>(state[offset]), prevValues, predictors, sampleCount);
		}    
    }

    void DecodeAndTallyObject(ArithmeticDecoder & decoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, uint8_t * state)
    {
        const int sampleCount = GetSampleCount(frameAdded);
        for(auto field : cl.fields)
		{
            int offset = stateOffset + field->dataOffset, prevValues[4];
            for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
		    reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field->uniqueId].DecodeAndTally(decoder, prevValues, predictors, sampleCount);
		}    
    }
};

Client::Client(const NCprotocol * protocol) : protocol(protocol)
{

}

std::shared_ptr<NCview> Client::CreateView(size_t classIndex, int uniqueId, int frameAdded)
{
    auto it = id2View.find(uniqueId);
    if(it != end(id2View))
    {
        if(auto ptr = it->second.lock())
        {
            assert(ptr->cl->uniqueId == classIndex);
            return ptr;
        }        
    }

    auto cl = protocol->classes[classIndex];
    auto ptr = std::make_shared<NCview>(this, cl, (int)stateAlloc.Allocate(cl->sizeInBytes), frameAdded);
    id2View[uniqueId] = ptr;
    return ptr;
}

void Client::ConsumeUpdate(ArithmeticDecoder & decoder)
{
    Frameset frameset;
    frameset.DecodeFrameList(decoder, *protocol);
    if(!frames.empty() && frames.rbegin()->first >= frameset.frame) return; // Don't bother decoding messages for old frames
    for(int i=0; i<4; ++i)
    {
        frameset.prevStates[i] = GetFrameState(frameset.prevFrames[i]);
        if(frameset.prevFrames[i] != 0 && frameset.prevStates[i] == nullptr) return; // Malformed packet
    }

    auto & distribs = frames[frameset.frame].distribs;
    auto & views = frames[frameset.frame].views;
    if(frameset.prevFrames[0] != 0)
    {
        distribs = frames[frameset.prevFrames[0]].distribs;
        views = frames[frameset.prevFrames[0]].views;
    }
    else distribs = Distribs(*protocol);

    // Decode indices of deleted objects
    int delObjects = distribs.delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, views.size());
        views[index].reset();
    }
    EraseIf(views, [](const std::shared_ptr<NCview> & v) { return !v; });

	// Decode classes of newly created objects, and instantiate corresponding views
	int newObjects = distribs.newObjectCountDist.DecodeAndTally(decoder);
	for (int i = 0; i < newObjects; ++i)
	{
        auto classIndex = distribs.classDist.DecodeAndTally(decoder);
        auto uniqueId = distribs.uniqueIdDist.DecodeAndTally(decoder);
        views.push_back(CreateView(classIndex, uniqueId, frameset.frame));
	}

    auto & state = frames[frameset.frame].state;
    state.resize(std::max(stateAlloc.GetTotalCapacity(),size_t(1)));

	// Decode updates for each view
	for(auto view : views) frameset.DecodeAndTallyObject(decoder, distribs, *view->cl, view->stateOffset, view->frameAdded, state.data());

    // Server will never again refer to frames before this point
    frames.erase(begin(frames), frames.lower_bound(std::min(frameset.frame - protocol->maxFrameDelta, frameset.prevFrames[3])));
    for(auto it = id2View.begin(); it != end(id2View); )
    {
        if(it->second.expired()) it = id2View.erase(it);
        else ++it;
    }
}

void Client::ProduceResponse(ArithmeticEncoder & encoder) const
{
    auto n = std::min(frames.size(), size_t(4));
    EncodeUniform(encoder, n, 5);
    auto it = frames.rbegin();
    for(int i=0; i<n; ++i, ++it) EncodeBits(encoder, it->first, 32);
}

////////////
// NCpeer //
////////////

NCpeer::NCpeer(NCauthority * auth) : auth(auth), nextId(1), client(auth->protocol)
{

}

NCpeer::~NCpeer()
{
    if(auth)
    {
        auto it = std::find(begin(auth->peers), end(auth->peers), this);
        if(it != end(auth->peers)) auth->peers.erase(it);
    }
}

void NCpeer::OnPublishFrame(int frame)
{
    if(!auth) return;

    for(auto change : visChanges)
    {
        auto it = std::find_if(begin(records), end(records), [=](ObjectRecord & r) { return r.object == change.first && r.IsLive(frame); });
        if((it != end(records)) == change.second) continue; // If object visibility is as desired, skip this change
        if(change.second) records.push_back({change.first, nextId++, frame, INT_MAX}); // Make object visible
        else it->frameRemoved = frame; // Make object invisible
    }
    visChanges.clear();

    int oldestAck = GetOldestAckFrame();
    EraseIf(records, [=](ObjectRecord & r) { return r.frameRemoved < oldestAck || r.frameRemoved < auth->frame - auth->protocol->maxFrameDelta; });
    frameDistribs.erase(begin(frameDistribs), frameDistribs.lower_bound(std::min(auth->frame - auth->protocol->maxFrameDelta, oldestAck)));
}

void NCpeer::SetVisibility(const NCobject * object, bool setVisible)
{
    if(!auth) return;
    visChanges.push_back({object,setVisible});
}

void NCpeer::ProduceUpdate(ArithmeticEncoder & encoder)
{
    Frameset frameset;
    frameset.frame = auth->frame;

    int32_t cutoff = frameset.frame - auth->protocol->maxFrameDelta;
    for(size_t i=0; i<4; ++i)
    {
        frameset.prevFrames[i] = ackFrames.size() > i ? ackFrames[i] : 0;
        if(frameset.prevFrames[i] < cutoff) frameset.prevFrames[i] = 0;
    }
    frameset.RefreshPredictors();

    // Encode frameset
    frameset.EncodeFrameList(encoder, *auth->protocol);

    auto & distribs = frameDistribs[frameset.frame];
    if(frameset.prevFrames[0] != 0) distribs = frameDistribs[frameset.prevFrames[0]];
    else distribs = Distribs(*auth->protocol);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const ObjectRecord *> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.IsLive(frameset.prevFrames[0]))
        {
            if(!record.IsLive(frameset.frame)) deletedIndices.push_back(index);  // If it has been removed, store its index
            ++index;                                                    // Either way, object was live, so it used an index
        }
        else if(record.IsLive(frameset.frame)) // If object was added between last frame and now
        {
            newObjects.push_back(&record);
        }
    }
    int numPrevObjects = index;
    distribs.delObjectCountDist.EncodeAndTally(encoder, deletedIndices.size());
    for(auto index : deletedIndices) EncodeUniform(encoder, index, numPrevObjects);

	// Encode classes of newly created objects
	distribs.newObjectCountDist.EncodeAndTally(encoder, newObjects.size());
	for (auto record : newObjects)
    {
        distribs.classDist.EncodeAndTally(encoder, record->object->cl->uniqueId);
        distribs.uniqueIdDist.EncodeAndTally(encoder, record->uniqueId);
    }

    auto state = auth->GetFrameState(frameset.frame);
    for(int i=0; i<4; ++i) frameset.prevStates[i] = auth->GetFrameState(frameset.prevFrames[i]);

	// Encode updates for each view
    for(const auto & record : records) if(record.IsLive(frameset.frame)) frameset.EncodeAndTallyObject(encoder, distribs, *record.object->cl, record.object->stateOffset, record.frameAdded, state);
}

void NCpeer::ConsumeResponse(ArithmeticDecoder & decoder) 
{
    if(!auth) return;
    std::vector<int> newAck;
    for(int i=0, n=DecodeUniform(decoder, 5); i!=n; ++i) newAck.push_back(DecodeBits(decoder, 32));
    if(newAck.empty()) return;
    if(ackFrames.empty() || ackFrames.front() < newAck.front()) ackFrames = newAck;
}

std::vector<uint8_t> NCpeer::ProduceMessage()
{ 
    if(!auth) return {};

    std::vector<uint8_t> buffer;
    ArithmeticEncoder encoder(buffer);
    client.ProduceResponse(encoder);
    ProduceUpdate(encoder);
    encoder.Finish();
    return buffer;
}

void NCpeer::ConsumeMessage(const void * data, int size)
{ 
    auto bytes = reinterpret_cast<const uint8_t *>(data);
    std::vector<uint8_t> buffer(bytes, bytes+size);
    ArithmeticDecoder decoder(buffer);
    ConsumeResponse(decoder);
    client.ConsumeUpdate(decoder);
}

////////////
// NCview //
////////////

NCview::NCview(Client * client, const NCclass * cl, int stateOffset, int frameAdded) : client(client), cl(cl), stateOffset(stateOffset), frameAdded(frameAdded)
{
    
}

NCview::~NCview()
{
    client->stateAlloc.Free(stateOffset, cl->sizeInBytes);
}

int NCview::GetIntField(const NCint * field) const
{ 
    if(field->cl != cl) return 0;
    return reinterpret_cast<const int &>(client->GetCurrentState()[stateOffset + field->dataOffset]); 
}