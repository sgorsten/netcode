#include "ViewInternal.h"

#include <algorithm>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value)
{
	return std::find(begin(vec), end(vec), value) - begin(vec);
}

template<class T, class F> void EraseIf(std::vector<T> & vec, F f)
{
    vec.erase(remove_if(begin(vec), end(vec), f), end(vec));
}

Policy::Policy(const VClass * classes, size_t numClasses) : numIntFields()
{
    for(size_t i=0; i<numClasses; ++i)
    {
        Class cl;
        cl.cl = classes[i];
        cl.index = i;
        cl.sizeBytes = 0;
        for(int fieldIndex = 0; fieldIndex < classes[i]->numIntFields; ++fieldIndex)
        {
            cl.fields.push_back({cl.sizeBytes, numIntFields++});
            cl.sizeBytes += sizeof(int);
        }
        this->classes.push_back(cl);
    }
}

VObject_::VObject_(VServer server, const Policy::Class & cl, int stateOffset) : server(server), cl(cl), stateOffset(stateOffset)
{
    
}

void VObject_::SetIntField(int index, int value)
{ 
    reinterpret_cast<int &>(server->state[stateOffset + cl.fields[index].offset]) = value; 
}

VServer_::VServer_(const VClass * classes, size_t numClasses) : policy(classes, numClasses), frame()
{

}

VPeer VServer_::CreatePeer()
{
	auto peer = new VPeer_(this);
	peers.push_back(peer);
	return peer;    
}

VObject VServer_::CreateObject(VClass objectClass)
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
}

VPeer_::VPeer_(VServer server) : server(server)
{

}

void VPeer_::OnPublishFrame(int frame)
{
    for(auto change : visChanges)
    {
        auto it = std::find_if(begin(records), end(records), [=](ObjectRecord & r) { return r.object == change.first && r.IsLive(frame); });
        if((it != end(records)) == change.second) continue; // If object visibility is as desired, skip this change
        if(change.second) records.push_back({change.first, frame, INT_MAX}); // Make object visible
        else it->frameRemoved = frame; // Make object invisible
    }
    visChanges.clear();

    int oldestAck = GetOldestAckFrame();
    EraseIf(records, [=](ObjectRecord & r) { return r.frameRemoved < oldestAck; });
    frameDistribs.erase(begin(frameDistribs), frameDistribs.find(oldestAck));
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

    // Encode frameset in plain ints, for now
	std::vector<uint8_t> bytes(12);
    memcpy(bytes.data() + 0, &frame, sizeof(int32_t));
    memcpy(bytes.data() + 4, &prevFrame, sizeof(int32_t));
    memcpy(bytes.data() + 8, &prevPrevFrame, sizeof(int32_t));

    // Prepare arithmetic code for this frame
	arith::Encoder encoder(bytes);
    auto & distribs = frameDistribs[frame];
    if(prevFrame != 0) distribs = frameDistribs[prevFrame];
    else distribs = Distribs(server->policy);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const VObject_ *> newObjects;
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
            newObjects.push_back(record.object);
        }
    }
    int numPrevObjects = index;
    distribs.delObjectCountDist.EncodeAndTally(encoder, deletedIndices.size());
    for(auto index : deletedIndices) EncodeUniform(encoder, index, numPrevObjects);

	// Encode classes of newly created objects
	distribs.newObjectCountDist.EncodeAndTally(encoder, newObjects.size());
	for (auto object : newObjects) distribs.classDist.EncodeAndTally(encoder, object->cl.index);

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

VView_::VView_(VClient client, const Policy::Class & cl, int stateOffset, int frameAdded) : client(client), cl(cl), stateOffset(stateOffset), frameAdded(frameAdded)
{
    
}

VView_::~VView_()
{
    client->stateAlloc.Free(stateOffset, cl.sizeBytes);
}

int VView_::GetIntField(int index) const
{ 
    return reinterpret_cast<const int &>(client->GetCurrentState()[stateOffset + cl.fields[index].offset]); 
}

VClient_::VClient_(const VClass * classes, size_t numClasses) : policy(classes, numClasses)
{

}

void VClient_::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
    if(bufferSize < 12) return;
    int32_t frame, prevFrame, prevPrevFrame;
    memcpy(&frame, buffer + 0, sizeof(int32_t));
    memcpy(&prevFrame, buffer + 4, sizeof(int32_t));
    memcpy(&prevPrevFrame, buffer + 8, sizeof(int32_t));

    // Don't bother decoding messages for old frames (TODO: We may still want to decode these frames if they can improve our ack set)
    if(!frames.empty() && frames.rbegin()->first >= frame) return;
    
    auto prevState = GetFrameState(prevFrame);
    auto prevPrevState = GetFrameState(prevPrevFrame);
    if(prevFrame != 0 && prevState == nullptr) return; // Malformed
    if(prevPrevFrame != 0 && prevPrevState == nullptr) return; // Malformed

    // Server will never again refer to frames before this point
    if(prevPrevState != 0) frames.erase(begin(frames), frames.find(prevPrevFrame));

    // Prepare arithmetic code for this frame
	std::vector<uint8_t> bytes(buffer + 12, buffer + bufferSize);
	arith::Decoder decoder(bytes);
    auto & distribs = frames[frame].distribs;
    auto & views = frames[frame].views;
    if(prevFrame != 0)
    {
        distribs = frames[prevFrame].distribs;
        views = frames[prevFrame].views;
    }
    else distribs = Distribs(policy);

    // Decode indices of deleted objects
    int delObjects = distribs.delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, views.size());
        views[index].reset();
    }
    EraseIf(views, [](const std::shared_ptr<VView_> & v) { return !v; });

	// Decode classes of newly created objects, and instantiate corresponding views
	int newObjects = distribs.newObjectCountDist.DecodeAndTally(decoder);
	for (int i = 0; i < newObjects; ++i)
	{
        auto & cl = policy.classes[distribs.classDist.DecodeAndTally(decoder)];
		views.push_back(std::make_shared<VView_>(this, cl, stateAlloc.Allocate(cl.sizeBytes), frame));
	}

    auto & state = frames[frame].state;
    state.resize(stateAlloc.GetTotalCapacity());

	// Decode updates for each view
	for (auto view : views)
	{
		for (auto & field : view->cl.fields)
		{
            int offset = view->stateOffset + field.offset;
            int prevValue = view->IsLive(prevFrame) ? reinterpret_cast<const int &>(prevState[offset]) : 0;
            int prevPrevValue = view->IsLive(prevPrevFrame) ? reinterpret_cast<const int &>(prevPrevState[offset]) : 0;
            reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field.distIndex].DecodeAndTally(decoder) + (prevValue * 2 - prevPrevValue);
		}
	}
}

std::vector<uint8_t> VClient_::ProduceResponse()
{
    std::vector<uint8_t> buffer;
    for(auto it = frames.rbegin(); it != frames.rend(); ++it)
    {
        auto offset = buffer.size();
        buffer.resize(offset + 4);
        memcpy(buffer.data() + offset, &it->first, sizeof(int32_t));
        if(buffer.size() == 8) break;
    }
    return buffer;
}