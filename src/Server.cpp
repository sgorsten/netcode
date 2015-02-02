#include "Server.h"

VObject_::VObject_(VServer_ * server, const Policy::Class & cl, int stateOffset) : server(server), cl(cl), stateOffset(stateOffset)
{
    
}

void VObject_::SetIntField(int index, int value)
{ 
    reinterpret_cast<int &>(server->state[stateOffset + cl.fields[index].offset]) = value; 
}

VServer_::VServer_(VClass_ * const * classes, size_t numClasses, int maxFrameDelta) : policy(classes, numClasses, maxFrameDelta), frame()
{

}

VPeer_ * VServer_::CreatePeer()
{
	auto peer = new VPeer_(this);
	peers.push_back(peer);
	return peer;    
}

VObject_ * VServer_::CreateObject(VClass_ * objectClass)
{
    for(auto & cl : policy.classes)
    {
        if(cl.cl == objectClass)
        {
	        auto object = new VObject_(this, cl, stateAlloc.Allocate(cl.sizeBytes));
            if(stateAlloc.GetTotalCapacity() > state.size()) state.resize(stateAlloc.GetTotalCapacity(), 0);
	        objects.push_back(object);
	        return object;
        }
    }
    return nullptr;
}

void VServer_::PublishFrame()
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
    if(oldestAck != 0) frameState.erase(begin(frameState), frameState.find(oldestAck));
    frameState.erase(begin(frameState), frameState.lower_bound(frame - policy.maxFrameDelta));
}

VPeer_::VPeer_(VServer_ * server) : server(server), nextId(1)
{

}

void VPeer_::OnPublishFrame(int frame)
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
    EraseIf(records, [=](ObjectRecord & r) { return r.frameRemoved < oldestAck || r.frameRemoved < server->frame - server->policy.maxFrameDelta; });
    frameDistribs.erase(begin(frameDistribs), frameDistribs.find(oldestAck));
    frameDistribs.erase(begin(frameDistribs), frameDistribs.lower_bound(server->frame - server->policy.maxFrameDelta));
}

void VPeer_::SetVisibility(const VObject_ * object, bool setVisible)
{
    visChanges.push_back({object,setVisible});
}

std::vector<uint8_t> VPeer_::ProduceUpdate()
{
    int32_t frame = server->frame;
    int32_t prevFrame = ackFrames.size() >= 1 ? ackFrames[ackFrames.size()-1] : 0;
    int32_t prevPrevFrame = ackFrames.size() >= 2 ? ackFrames[ackFrames.size()-2] : 0;
    int32_t cutoff = frame - server->policy.maxFrameDelta;
    if(prevFrame < cutoff) prevFrame = 0;
    if(prevPrevFrame < cutoff) prevPrevFrame = 0;    

    // Encode frameset in plain ints, for now
	std::vector<uint8_t> bytes(4);
    memcpy(bytes.data() + 0, &frame, sizeof(int32_t));

    // Prepare arithmetic code for this frame
	arith::Encoder encoder(bytes);
    EncodeUniform(encoder, prevFrame ? frame - prevFrame : 0, server->policy.maxFrameDelta+1);
    EncodeUniform(encoder, prevPrevFrame ? frame - prevPrevFrame : 0, server->policy.maxFrameDelta+1);
    
    auto & distribs = frameDistribs[frame];
    if(prevFrame != 0) distribs = frameDistribs[prevFrame];
    else distribs = Distribs(server->policy);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const ObjectRecord *> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.IsLive(prevFrame))
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
        distribs.classDist.EncodeAndTally(encoder, record->object->cl.index);
        distribs.uniqueIdDist.EncodeAndTally(encoder, record->uniqueId);
    }

    auto state = server->GetFrameState(frame);
    auto prevState = server->GetFrameState(prevFrame);
    auto prevPrevState = server->GetFrameState(prevPrevFrame);

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.IsLive(frame)) continue;
        auto object = record.object;

        for(auto & field : object->cl.fields)
		{
            int offset = object->stateOffset + field.offset;
            int value = reinterpret_cast<const int &>(state[offset]);
            int prevValue = record.IsLive(prevFrame) ? reinterpret_cast<const int &>(prevState[offset]) : 0;
            int prevPrevValue = record.IsLive(prevPrevFrame) ? reinterpret_cast<const int &>(prevPrevState[offset]) : 0;
			distribs.intFieldDists[field.distIndex].EncodeAndTally(encoder, value - prevValue * 2 + prevPrevValue);
		}
	}

	encoder.Finish();
	return bytes;
}

void VPeer_::ConsumeResponse(const uint8_t * data, size_t size) 
{
    std::vector<int> newAck;
    while(size >= 4)
    {
        int32_t frame;
        memcpy(&frame, data, sizeof(frame));
        newAck.push_back(frame);
        size -= 4;
    }
    if(newAck.empty()) return;
    if(ackFrames.empty() || ackFrames.front() < newAck.front()) ackFrames = newAck;
}