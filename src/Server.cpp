#include "Server.h"

using namespace netcode;

NCobject::NCobject(NCserver * server, const NCclass * cl, int stateOffset) : server(server), cl(cl), stateOffset(stateOffset)
{
    
}

void NCobject::SetIntField(const NCint * field, int value)
{ 
    if(field->cl != cl) return;
    reinterpret_cast<int &>(server->state[stateOffset + field->dataOffset]) = value; 
}

void NCobject::Destroy()
{
    server->stateAlloc.Free(stateOffset, cl->sizeInBytes);
    for(auto peer : server->peers) peer->SetVisibility(this, false);
    auto it = std::find(begin(server->objects), end(server->objects), this);
    if(it != end(server->objects)) server->objects.erase(it);
    delete this;
}

NCserver::NCserver(const NCprotocol * protocol) : protocol(protocol), frame()
{

}

NCpeer * NCserver::CreatePeer()
{
	auto peer = new NCpeer(this);
	peers.push_back(peer);
	return peer;    
}

NCobject * NCserver::CreateObject(const NCclass * cl)
{
    if(cl->protocol != protocol) return nullptr;

	auto object = new NCobject(this, cl, stateAlloc.Allocate(cl->sizeInBytes));
    if(stateAlloc.GetTotalCapacity() > state.size()) state.resize(stateAlloc.GetTotalCapacity(), 0);
	objects.push_back(object);
	return object;
}

void NCserver::PublishFrame()
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

NCpeer::NCpeer(NCserver * server) : server(server), nextId(1)
{

}

void NCpeer::OnPublishFrame(int frame)
{
    for(auto change : visChanges)
    {
        auto it = std::find_if(begin(records), end(records), [=](ObjectRecord & r) { return r.object == change.first && r.IsLive(frame); });
        if((it != end(records)) == change.second) continue; // If object visibility is as desired, skip this change
        if(change.second) records.push_back({change.first, nextId++, frame, INT_MAX}); // Make object visible
        else it->frameRemoved = frame; // Make object invisible
    }
    visChanges.clear();

    int oldestAck = GetOldestAckFrame();
    EraseIf(records, [=](ObjectRecord & r) { return r.frameRemoved < oldestAck || r.frameRemoved < server->frame - server->protocol->maxFrameDelta; });
    frameDistribs.erase(begin(frameDistribs), frameDistribs.lower_bound(std::min(server->frame - server->protocol->maxFrameDelta, oldestAck)));
}

void NCpeer::SetVisibility(const NCobject * object, bool setVisible)
{
    visChanges.push_back({object,setVisible});
}

std::vector<uint8_t> NCpeer::ProduceUpdate()
{
    int32_t frame = server->frame, cutoff = frame - server->protocol->maxFrameDelta;
    int32_t prevFrames[4];
    for(int i=0; i<4; ++i)
    {
        prevFrames[i] = ackFrames.size() > i ? ackFrames[i] : 0;
        if(prevFrames[i] < cutoff) prevFrames[i] = 0;
    }

    CurvePredictor predictors[5];
    predictors[1] = prevFrames[0] != 0 ? MakeConstantPredictor() : predictors[0];
    predictors[2] = prevFrames[1] != 0 ? MakeLinearPredictor(frame-prevFrames[0], frame-prevFrames[1]) : predictors[1];
    predictors[3] = prevFrames[2] != 0 ? MakeQuadraticPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2]) : predictors[1];
    predictors[4] = prevFrames[3] != 0 ? MakeCubicPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2], frame-prevFrames[3]) : predictors[1];

    // Encode frameset in plain ints, for now
	std::vector<uint8_t> bytes(4);
    memcpy(bytes.data() + 0, &frame, sizeof(int32_t));

    // Prepare arithmetic code for this frame
	ArithmeticEncoder encoder(bytes);
    for(int i=0; i<4; ++i) EncodeUniform(encoder, prevFrames[i] ? frame - prevFrames[i] : 0, server->protocol->maxFrameDelta+1);

    auto & distribs = frameDistribs[frame];
    if(prevFrames[0] != 0) distribs = frameDistribs[prevFrames[0]];
    else distribs = Distribs(*server->protocol);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const ObjectRecord *> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.IsLive(prevFrames[0]))
        {
            if(!record.IsLive(frame)) deletedIndices.push_back(index);  // If it has been removed, store its index
            ++index;                                                    // Either way, object was live, so it used an index
        }
        else if(record.IsLive(frame)) // If object was added between last frame and now
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

    auto state = server->GetFrameState(frame);
    const uint8_t * prevStates[4];
    for(int i=0; i<4; ++i) prevStates[i] = server->GetFrameState(prevFrames[i]);

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.IsLive(frame)) continue;
        auto object = record.object;

        int sampleCount = 0;
        for(int i=4; i>0; --i)
        {
            if(record.IsLive(prevFrames[i-1]))
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
			distribs.intFieldDists[field->uniqueId].EncodeAndTally(encoder, value, prevValues, predictors, sampleCount);
		}
	}

	encoder.Finish();
	return bytes;
}

void NCpeer::ConsumeResponse(const uint8_t * data, size_t size) 
{
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