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
    CurvePredictor predictors[5];

    void RefreshPredictors()
    {
        predictors[0] = CurvePredictor();
        predictors[1] = prevFrames[0] != 0 ? MakeConstantPredictor() : predictors[0];
        predictors[2] = prevFrames[1] != 0 ? MakeLinearPredictor(frame-prevFrames[0], frame-prevFrames[1]) : predictors[1];
        predictors[3] = prevFrames[2] != 0 ? MakeQuadraticPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2]) : predictors[1];
        predictors[4] = prevFrames[3] != 0 ? MakeCubicPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2], frame-prevFrames[3]) : predictors[1];
    }

    void Decode(ArithmeticDecoder & decoder, const NCprotocol & protocol)
    {
        frame = DecodeBits(decoder, 32);
        for(int i=0; i<4; ++i)
        {
            code_t value = DecodeUniform(decoder, protocol.maxFrameDelta+1);
            prevFrames[i] = value ? frame - value : 0;
        }
        RefreshPredictors();
    }

    void Encode(ArithmeticEncoder & encoder, const NCprotocol & protocol)
    {
        EncodeBits(encoder, frame, 32);
        for(int i=0; i<4; ++i)
        {
            code_t value = prevFrames[i] ? frame - prevFrames[i] : 0;
            EncodeUniform(encoder, value, protocol.maxFrameDelta+1);
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

void Client::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
    if(bufferSize < 4) return;
    const uint8_t * prevStates[4];

	std::vector<uint8_t> bytes(buffer, buffer + bufferSize);
	ArithmeticDecoder decoder(bytes);

    Frameset frameset;
    frameset.Decode(decoder, *protocol);
    if(!frames.empty() && frames.rbegin()->first >= frameset.frame) return; // Don't bother decoding messages for old frames
    for(int i=0; i<4; ++i)
    {
        prevStates[i] = GetFrameState(frameset.prevFrames[i]);
        if(frameset.prevFrames[i] != 0 && prevStates[i] == nullptr) return; // Malformed packet
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
	for (auto view : views)
	{
        int sampleCount = 0;
        for(int i=4; i>0; --i)
        {
            if(view->IsLive(frameset.prevFrames[i-1]))
            {
                sampleCount = i;
                break;
            }
        }

		for (auto field : view->cl->fields)
		{
            int offset = view->stateOffset + field->dataOffset;
            int prevValues[4];
            for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
            reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field->uniqueId].DecodeAndTally(decoder, prevValues, frameset.predictors, sampleCount);
		}
	}

    // Server will never again refer to frames before this point
    frames.erase(begin(frames), frames.lower_bound(std::min(frameset.frame - protocol->maxFrameDelta, frameset.prevFrames[3])));
    for(auto it = id2View.begin(); it != end(id2View); )
    {
        if(it->second.expired()) it = id2View.erase(it);
        else ++it;
    }
}

std::vector<uint8_t> Client::ProduceResponse() const
{
    std::vector<uint8_t> buffer;
    for(auto it = frames.rbegin(); it != frames.rend(); ++it)
    {
        auto offset = buffer.size();
        buffer.resize(offset + 4);
        memcpy(buffer.data() + offset, &it->first, sizeof(int32_t));
        if(buffer.size() == 16) break;
    }
    return buffer;
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

std::vector<uint8_t> NCpeer::ProduceUpdate()
{
    if(!auth) return {};

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
	std::vector<uint8_t> bytes;
    ArithmeticEncoder encoder(bytes);
    frameset.Encode(encoder, *auth->protocol);

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
    const uint8_t * prevStates[4];
    for(int i=0; i<4; ++i) prevStates[i] = auth->GetFrameState(frameset.prevFrames[i]);

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.IsLive(frameset.frame)) continue;
        auto object = record.object;

        int sampleCount = 0;
        for(int i=4; i>0; --i)
        {
            if(record.IsLive(frameset.prevFrames[i-1]))
            {
                sampleCount = i;
                break;
            }
        }

        for(auto field : object->cl->fields)
		{
            int offset = object->stateOffset + field->dataOffset;
            int value = reinterpret_cast<const int &>(state[offset]);
            int prevValues[4];
            for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
			distribs.intFieldDists[field->uniqueId].EncodeAndTally(encoder, value, prevValues, frameset.predictors, sampleCount);
		}
	}

	encoder.Finish();
	return bytes;
}

void NCpeer::ConsumeResponse(const uint8_t * data, size_t size) 
{
    if(!auth) return;
    std::vector<int> newAck;
    while(size >= 4)
    {
        int32_t frame;
        memcpy(&frame, data, sizeof(frame));
        newAck.push_back(frame);
        data += 4;
        size -= 4;
    }
    if(newAck.empty()) return;
    if(ackFrames.empty() || ackFrames.front() < newAck.front()) ackFrames = newAck;
}

std::vector<uint8_t> NCpeer::ProduceMessage()
{ 
    auto response = client.ProduceResponse();
    auto update = ProduceUpdate();
    std::vector<uint8_t> buffer;
    buffer.resize(2);
    *reinterpret_cast<uint16_t *>(buffer.data()) = response.size();
    buffer.insert(end(buffer), begin(response), end(response));
    buffer.insert(end(buffer), begin(update), end(update));
    return buffer;
}

void NCpeer::ConsumeMessage(const void * data, int size)
{ 
    auto bytes = reinterpret_cast<const uint8_t *>(data);
    auto responseSize = *reinterpret_cast<const uint16_t *>(data);
    ConsumeResponse(bytes + 2, responseSize);
    client.ConsumeUpdate(bytes + 2 + responseSize, size - 2 - responseSize);
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