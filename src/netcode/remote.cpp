// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "object.h"
#include <cassert>

using namespace netcode;

///////////////
// RemoteSet //
///////////////

struct RemoteSet::Object : public NCobject
{
    NCpeer * peer;
    int uniqueId;
    const NCclass * cl;
    int frameAdded;
    std::vector<uint8_t> constState;
	int varStateOffset;
    
	Object(NCpeer * peer, int uniqueId, const NCclass * cl, int frameAdded, std::vector<uint8_t> constState) : 
        peer(peer), uniqueId(uniqueId), cl(cl), frameAdded(frameAdded), constState(move(constState)), varStateOffset(peer->remote.stateAlloc.Allocate(cl->varSizeInBytes)) {}
    ~Object()
    {
        peer->auth->PurgeReferencesToObject(this);
        peer->remote.stateAlloc.Free(varStateOffset, cl->varSizeInBytes);    
    }

    bool IsLive(int frame) const { return frameAdded <= frame; }
    const NCclass * GetClass() const override { return cl; }

    int GetInt(const NCint * field) const override
    { 
        if(field->cl != cl) return 0;
        if(field->isConst) return reinterpret_cast<const int &>(constState[field->dataOffset]); 
        return reinterpret_cast<const int &>(peer->remote.frameStates.rbegin()->second[varStateOffset + field->dataOffset]); 
    }

    const NCobject * GetRef(const NCref * field) const override
    { 
        if(field->cl != cl) return nullptr;
        auto id = reinterpret_cast<const int &>(peer->remote.frameStates.rbegin()->second[varStateOffset + field->dataOffset]);
        if(id > 0) return peer->remote.GetObjectFromUniqueId(id); // Positive IDs refer to other remote objects
        if(id < 0) return peer->local.GetObjectFromUniqueId(-id); // Negative IDs refer to our own local objects
        return nullptr;                                           // Zero refers to nullptr
    }
};

struct RemoteSet::Frame
{
    std::vector<std::shared_ptr<Object>> views;
    Distribs distribs;
};

RemoteSet::~RemoteSet()
{

}

RemoteSet::RemoteSet(const NCprotocol * protocol) : protocol(protocol)
{

}

int RemoteSet::GetObjectCount() const
{
    if(frames.empty()) return 0;
    return frames.rbegin()->second.views.size() + events.size();        
}

const NCobject * RemoteSet::GetObjectFromIndex(int index) const
{ 
    if(frames.empty()) return nullptr;
    auto & frame = frames.rbegin()->second;
    if(index < frame.views.size()) return frame.views[index].get();
    return events[index - frame.views.size()].get();
}

const NCobject * RemoteSet::GetObjectFromUniqueId(int uniqueId) const
{
    auto it = id2View.find(uniqueId);
    if(it == end(id2View)) return nullptr;
    auto view = it->second.lock();
    for(const auto & v : frames.rbegin()->second.views) if(v.get() == view.get()) return v.get();
    return nullptr;
}

int RemoteSet::GetUniqueIdFromObject(const NCobject * object) const
{
    if(frames.empty()) return 0;
    for(auto & view : frames.rbegin()->second.views)
    {
        if(view.get() == object) return view->uniqueId;
    }
    return 0;
}

void RemoteSet::ConsumeUpdate(ArithmeticDecoder & decoder, NCpeer * peer)
{
    const int mostRecentFrame = frames.empty() ? 0 : frames.rbegin()->first;
    
    // Decode frameset
    const Frameset frameset(netcode::DecodeFramelist(decoder, 5, protocol->maxFrameDelta), frameStates);
    if(!frames.empty() && frames.rbegin()->first >= frameset.GetCurrentFrame()) return; // Don't bother decoding messages for old frames
    //for(int i=0; i<4; ++i) if(frameset.prevFrames[i] != 0 && frameset.prevStates[i] == nullptr) return; // Malformed packet

    // Prepare probability distributions
    auto & frame = frames[frameset.GetCurrentFrame()];
    if(frameset.GetPreviousFrame() != 0) frame = frames[frameset.GetPreviousFrame()];
    else frame.distribs = Distribs(*protocol);

    // Decode events that occurred in each frame between the last acknowledged frame and the current frame
    events.clear();
    for(int i=frameset.GetPreviousFrame()+1; i<=frameset.GetCurrentFrame(); ++i)
    {
        // All of the events decoded in here happen on frame i
        for(int j=0, n = frame.distribs.eventCountDist.DecodeAndTally(decoder); j<n; ++j)
        {
            auto classIndex = frame.distribs.eventClassDist.DecodeAndTally(decoder);
            auto cl = protocol->eventClasses[classIndex];
            auto state = frame.distribs.DecodeAndTallyObjectConstants(decoder, *cl);
            if(i > mostRecentFrame) // Only generate an event once (it will likely be sent multiple times before being acknowledged)
            {
                events.push_back(std::unique_ptr<Object>(new Object(peer, 0, cl, i, std::move(state))));
            }
        }
    }

    // Decode indices of deleted objects
    int delObjects = frame.distribs.delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, frame.views.size());
        frame.views[index].reset();
    }
    EraseIf(frame.views, [](const std::shared_ptr<Object> & v) { return !v; });

	// Decode classes of newly created objects, and instantiate corresponding views
	int newObjects = frame.distribs.newObjectCountDist.DecodeAndTally(decoder);
	for (int i = 0; i < newObjects; ++i)
	{
        auto classIndex = frame.distribs.objectClassDist.DecodeAndTally(decoder);
        auto uniqueId = frame.distribs.uniqueIdDist.DecodeAndTally(decoder);
        auto state = frame.distribs.DecodeAndTallyObjectConstants(decoder, *protocol->objectClasses[classIndex]);

        auto it = id2View.find(uniqueId);
        auto ptr = it != end(id2View) ? it->second.lock() : nullptr;
        if(!ptr)
        {
            ptr = std::make_shared<Object>(peer, uniqueId, protocol->objectClasses[classIndex], frameset.GetCurrentFrame(), move(state));
            id2View[uniqueId] = ptr;
        }
        frame.views.push_back(ptr);
	}

    auto & state = frameStates[frameset.GetCurrentFrame()];
    state.resize(std::max(stateAlloc.GetTotalCapacity(),size_t(1)));

	// Decode updates for each view
	for(auto view : frame.views)
    {
        frameset.DecodeAndTallyObject(decoder, frame.distribs, *view->cl, view->varStateOffset, view->frameAdded, state.data());
    }

    // Server will never again refer to frames before this point
    int lastFrameToKeep = std::min(frameset.GetCurrentFrame() - protocol->maxFrameDelta, frameset.GetEarliestFrame());
    EraseBefore(frames, lastFrameToKeep);
    EraseBefore(frameStates, lastFrameToKeep);
    for(auto it = id2View.begin(); it != end(id2View); )
    {
        if(it->second.expired()) it = id2View.erase(it);
        else ++it;
    }
}

void RemoteSet::ProduceResponse(ArithmeticEncoder & encoder) const
{
    auto n = std::min(frames.size(), size_t(4));
    int ackFrames[4];
    auto it = frames.rbegin();
    for(size_t i=0; i<n; ++i, ++it) ackFrames[i] = it->first;
    netcode::EncodeFramelist(encoder, ackFrames, n, 4, protocol->maxFrameDelta);
}