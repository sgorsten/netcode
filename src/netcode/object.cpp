// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "object.h"
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

LocalObject * NCauthority::CreateObject(const NCclass * cl)
{
    if(cl->protocol != protocol) return nullptr;

    if(cl->isEvent)
    {
        auto event = new LocalObject(this, cl);
        events.push_back(event);
        return event;
    }
    else
    {
	    auto object = new LocalObject(this, cl);
        if(stateAlloc.GetTotalCapacity() > state.size()) state.resize(stateAlloc.GetTotalCapacity(), 0);
	    objects.push_back(object);
	    return object;
    }
}

void NCauthority::PurgeReferencesToObject(NCobject * object)
{
    for(auto obj : objects)
    {
        for(auto field : obj->cl->varRefs)
        {
            auto & ref = reinterpret_cast<NCobject * &>(state[obj->varStateOffset + field->dataOffset]);
            if(ref == object) ref = nullptr;
        }
    }
}

void NCauthority::PublishFrame()
{
    // Publish object state
    ++frame;
    for(auto obj : objects) obj->isPublished = true;
    frameState[frame] = state;

    // Publish events which occurred this frame
    for(auto ev : events) ev->isPublished = true;
    eventHistory[frame] = std::move(events);
    events.clear();

    // Publish visibility changes and such
    int oldestAck = INT_MAX;
    for(auto peer : peers)
    {
        peer->OnPublishFrame(frame);
        oldestAck = std::min(oldestAck, peer->GetOldestAckFrame());
    }

    // Once all clients have acknowledged a certain frame, expire all older frames
    auto lastFrameToKeep = std::min(frame - protocol->maxFrameDelta, oldestAck);
    EraseBefore(frameState, lastFrameToKeep);

    for(auto p : eventHistory)
    {
        if(p.first >= lastFrameToKeep) break;
        for(auto e : p.second)
        {
            for(auto peer : peers) peer->SetVisibility(e, false);
            delete e;
        }
    }
    EraseBefore(eventHistory, lastFrameToKeep);
}

//////////////
// NCobject //
//////////////

LocalObject::LocalObject(NCauthority * auth, const NCclass * cl) : 
    auth(auth), cl(cl), constState(cl->constSizeInBytes), varStateOffset(auth->stateAlloc.Allocate(cl->varSizeInBytes)), isPublished(false) 
{

}

int LocalObject::GetInt(const NCint * field) const
{
    if(field->cl != cl) return 0;
    if(!field->isConst) return reinterpret_cast<const int &>(auth->state[varStateOffset + field->dataOffset]);
    return reinterpret_cast<const int &>(constState[field->dataOffset]);
}

const NCobject * LocalObject::GetRef(const NCref * field) const
{
    if(field->cl != cl) return nullptr;
    return reinterpret_cast<const NCobject * const &>(auth->state[varStateOffset + field->dataOffset]);
}

void LocalObject::SetVisibility(NCpeer * peer, bool isVisible) const
{
    peer->SetVisibility(this, isVisible); 
}

void LocalObject::SetInt(const NCint * field, int value)
{ 
    if(field->cl != cl) return;
    if(!field->isConst) reinterpret_cast<int &>(auth->state[varStateOffset + field->dataOffset]) = value; 
    else if(!isPublished) reinterpret_cast<int &>(constState[field->dataOffset]) = value;
}

void LocalObject::SetRef(const NCref * field, const NCobject * value)
{ 
    if(field->cl != cl) return;
    reinterpret_cast<const NCobject * &>(auth->state[varStateOffset + field->dataOffset]) = value; 
}

void LocalObject::Destroy()
{ 
    if(cl->isEvent)
    {
        if(!isPublished)
        {
            auth->PurgeReferencesToObject(this); // TODO: Prevent taking references to events in the first place
            for(auto peer : auth->peers) peer->SetVisibility(this, false);
            Erase(auth->events, this);
            auth->events.erase(std::find(begin(auth->events), end(auth->events), this));
            delete this;
        }
    }
    else
    {
        auth->PurgeReferencesToObject(this);
        auth->stateAlloc.Free(varStateOffset, cl->varSizeInBytes);
        for(auto peer : auth->peers) peer->SetVisibility(this, false);
        Erase(auth->objects, this);
        delete this; 
    }
}

////////////
// NCpeer //
////////////

NCpeer::NCpeer(NCauthority * auth) : auth(auth), nextId(1), remote(auth->protocol)
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

void NCpeer::SetVisibility(const LocalObject * object, bool setVisible)
{
    if(!auth) return;

    if(object->cl->isEvent)
    {
        if(object->isPublished) return;
        if(setVisible) visibleEvents.insert(object);
        else visibleEvents.erase(object);
    }
    else
    {
        visChanges.push_back({object,setVisible});
    }
}

int NCpeer::GetNetId(const NCobject * object, int frame) const
{
    // First check to see if this is a local object, in which case, send a positive ID
    for(auto & record : records)
    {
        if(record.object == object && record.IsLive(frame))
        {
            return record.uniqueId;
        }
    }

    // Next, check to see if this is a remote object, in which case, send a negative ID
    if(auto id = remote.GetUniqueIdFromObject(object)) return -id;

    // Otherwise, send a 0, to indicate nullptr
    return 0;
}

void NCpeer::ProduceUpdate(ArithmeticEncoder & encoder)
{
    std::vector<int> frameList = {auth->frame};
    int32_t cutoff = auth->frame - auth->protocol->maxFrameDelta;
    for(auto frame : ackFrames) if(frame >= cutoff) frameList.push_back(frame); // TODO: Enforce this in PublishFrame instead

    netcode::EncodeFramelist(encoder, frameList.data(), frameList.size(), 5, auth->protocol->maxFrameDelta);
    const Frameset frameset(frameList, auth->frameState);

    // Obtain probability distributions for this frame
    auto & distribs = frameDistribs[frameset.GetCurrentFrame()];
    if(frameset.GetPreviousFrame() != 0) distribs = frameDistribs[frameset.GetPreviousFrame()];
    else distribs = Distribs(*auth->protocol);

    // Encode visible events that occurred in each frame between the last acknowledged frame and the current frame
    std::vector<const LocalObject *> sendEvents;
    for(int i=frameset.GetPreviousFrame()+1; i<=frameset.GetCurrentFrame(); ++i)
    {
        sendEvents.clear();
        for(auto e : auth->eventHistory.find(i)->second) if(visibleEvents.find(e) != end(visibleEvents)) sendEvents.push_back(e);
        distribs.eventCountDist.EncodeAndTally(encoder, sendEvents.size());
        for(auto e : sendEvents)
        {
            distribs.eventClassDist.EncodeAndTally(encoder, e->cl->uniqueId);
            distribs.EncodeAndTallyObjectConstants(encoder, *e->cl, e->constState);
        }
    }

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const ObjectRecord *> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.IsLive(frameset.GetPreviousFrame()))
        {
            if(!record.IsLive(frameset.GetCurrentFrame())) deletedIndices.push_back(index);  // If it has been removed, store its index
            ++index;                                                    // Either way, object was live, so it used an index
        }
        else if(record.IsLive(frameset.GetCurrentFrame())) // If object was added between last frame and now
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
        distribs.objectClassDist.EncodeAndTally(encoder, record->object->cl->uniqueId);
        distribs.uniqueIdDist.EncodeAndTally(encoder, record->uniqueId);
        distribs.EncodeAndTallyObjectConstants(encoder, *record->object->cl, record->object->constState);
    }

	// Encode updates for each view
    auto state = auth->GetFrameState(frameset.GetCurrentFrame());
    for(const auto & record : records)
    {
        if(record.IsLive(frameset.GetCurrentFrame()))
        {
            frameset.EncodeAndTallyObject(encoder, distribs, *record.object->cl, record.object->varStateOffset, record.frameAdded, state);

            for(auto field : record.object->cl->varRefs)
            {
                auto offset = record.object->varStateOffset + field->dataOffset;
                auto value = reinterpret_cast<const NCobject * const &>(state[offset]);
                auto prevValue = record.IsLive(frameset.GetPreviousFrame()) ? reinterpret_cast<const NCobject * const &>(auth->GetFrameState(frameset.GetPreviousFrame())[offset]) : nullptr;
                auto id = GetNetId(value, frameset.GetCurrentFrame());
                auto prevId = GetNetId(prevValue, frameset.GetPreviousFrame());
                distribs.uniqueIdDist.EncodeAndTally(encoder, id-prevId);
            }
        }
    }
}

void NCpeer::ConsumeResponse(ArithmeticDecoder & decoder) 
{
    if(!auth) return;
    auto newAck = netcode::DecodeFramelist(decoder, 4, auth->protocol->maxFrameDelta);
    if(newAck.empty()) return;
    if(ackFrames.empty() || ackFrames.front() < newAck.front()) ackFrames = newAck;
}

std::vector<uint8_t> NCpeer::ProduceMessage()
{ 
    if(!auth) return {};

    std::vector<uint8_t> buffer;
    ArithmeticEncoder encoder(buffer);
    remote.ProduceResponse(encoder);
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
    remote.ConsumeUpdate(decoder, this);
}
