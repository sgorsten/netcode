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
    for(auto peer : peers) peer->OnPublishFrame(frame);

    // Expire old frames
    frameState.erase(frame-3);
}

VPeer_::VPeer_(VServer server) : server(server)
{
    intFieldDists.resize(server->policy.numIntFields);
    prevFrame = 0;
    frame = 1;
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
    this->frame = frame;

    EraseIf(records, [=](ObjectRecord & r) { return r.frameRemoved < frame-2; });
}

void VPeer_::SetVisibility(const VObject_ * object, bool setVisible)
{
    visChanges.push_back({object,setVisible});
}

std::vector<uint8_t> VPeer_::ProduceUpdate()
{
	std::vector<uint8_t> bytes;
	arith::Encoder encoder(bytes);

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
    delObjectCountDist.EncodeAndTally(encoder, deletedIndices.size());
    for(auto index : deletedIndices) EncodeUniform(encoder, index, numPrevObjects);

	// Encode classes of newly created objects
	newObjectCountDist.EncodeAndTally(encoder, newObjects.size());
	for (auto object : newObjects) EncodeUniform(encoder, object->cl.index, server->policy.classes.size());

    auto state = server->GetFrameState(frame);
    auto prevState = server->GetFrameState(frame-1);
    auto prevPrevState = server->GetFrameState(frame-2);

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.IsLive(frame)) continue;
        auto object = record.object;

        for(auto & field : object->cl.fields)
		{
            int offset = object->stateOffset + field.offset;
            int value = reinterpret_cast<const int &>(state[offset]);
            int prevValue = record.IsLive(frame-1) ? reinterpret_cast<const int &>(prevState[offset]) : 0;
            int prevPrevValue = record.IsLive(frame-2) ? reinterpret_cast<const int &>(prevPrevState[offset]) : 0;
			intFieldDists[field.distIndex].EncodeAndTally(encoder, value - prevValue * 2 + prevPrevValue);
		}
	}

    prevFrame = frame;

	encoder.Finish();
	return bytes;
}

VView_::VView_(VClient client, const Policy::Class & cl, int stateOffset, int frameAdded) : client(client), cl(cl), stateOffset(stateOffset), frameAdded(frameAdded)
{
    
}

int VView_::GetIntField(int index) const
{ 
    return reinterpret_cast<const int &>(client->GetCurrentState()[stateOffset + cl.fields[index].offset]); 
}

VClient_::VClient_(const VClass * classes, size_t numClasses) : policy(classes, numClasses), intFieldDists(policy.numIntFields), frame()
{

}

void VClient_::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
	std::vector<uint8_t> bytes(buffer, buffer + bufferSize);
	arith::Decoder decoder(bytes);

    ++frame;

    // Decode indices of deleted objects
    int delObjects = delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, views.size());
        stateAlloc.Free(views[index]->stateOffset, views[index]->cl.sizeBytes);
        delete views[index];
        views[index] = 0;
    }
    EraseIf(views, [](VView v) { return !v; });

	// Decode classes of newly created objects, and instantiate corresponding views
	int newObjects = newObjectCountDist.DecodeAndTally(decoder);
	for (int i = 0; i < newObjects; ++i)
	{
        auto & cl = policy.classes[DecodeUniform(decoder, policy.classes.size())];
		views.push_back(new VView_(this, cl, stateAlloc.Allocate(cl.sizeBytes), frame));
	}

    auto & state = frameState[frame];
    state.resize(stateAlloc.GetTotalCapacity());
    auto prevState = GetFrameState(frame-1);
    auto prevPrevState = GetFrameState(frame-2);

	// Decode updates for each view
	for (auto view : views)
	{
		for (auto & field : view->cl.fields)
		{
            int offset = view->stateOffset + field.offset;
            int prevValue = view->IsLive(frame-1) ? reinterpret_cast<const int &>(prevState[offset]) : 0;
            int prevPrevValue = view->IsLive(frame-2) ? reinterpret_cast<const int &>(prevPrevState[offset]) : 0;
            reinterpret_cast<int &>(state[offset]) = intFieldDists[field.distIndex].DecodeAndTally(decoder) + (prevValue * 2 - prevPrevValue);
		}
	}

    frameState.erase(frame-3);
}

std::vector<uint8_t> VClient_::ProduceResponse()
{ 
    std::vector<uint8_t> buffer;
    arith::Encoder encoder(buffer);
    encoder.Finish();
    return buffer;
}