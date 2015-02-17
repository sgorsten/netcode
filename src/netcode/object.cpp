// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "implementation.h"
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
        peer->local.PurgeReferences();
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
        peer->local.OnPublishFrame(frame);
        oldestAck = std::min(oldestAck, peer->local.GetOldestAckFrame());
    }

    // Once all clients have acknowledged a certain frame, expire all older frames
    auto lastFrameToKeep = std::min(frame - protocol->maxFrameDelta, oldestAck);
    EraseBefore(frameState, lastFrameToKeep);

    for(auto p : eventHistory)
    {
        if(p.first >= lastFrameToKeep) break;
        for(auto e : p.second)
        {
            for(auto peer : peers) peer->local.SetVisibility(e, false);
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
    peer->local.SetVisibility(this, isVisible); 
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
            for(auto peer : auth->peers) peer->local.SetVisibility(this, false);
            Erase(auth->events, this);
            auth->events.erase(std::find(begin(auth->events), end(auth->events), this));
            delete this;
        }
    }
    else
    {
        auth->PurgeReferencesToObject(this);
        auth->stateAlloc.Free(varStateOffset, cl->varSizeInBytes);
        for(auto peer : auth->peers) peer->local.SetVisibility(this, false);
        Erase(auth->objects, this);
        delete this; 
    }
}

////////////
// NCpeer //
////////////

NCpeer::NCpeer(NCauthority * auth) : auth(auth), local(auth), remote(auth->protocol)
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

int NCpeer::GetNetId(const NCobject * object, int frame) const
{
    if(auto id = local.GetUniqueIdFromObject(object, frame)) return id; // First check to see if this is a local object, in which case, send a positive ID
    if(auto id = remote.GetUniqueIdFromObject(object)) return -id;      // Next, check to see if this is a remote object, in which case, send a negative ID
    return 0;                                                           // Otherwise, send a 0, to indicate nullptr
}


std::vector<uint8_t> NCpeer::ProduceMessage()
{ 
    if(!auth) return {};

    std::vector<uint8_t> buffer;
    ArithmeticEncoder encoder(buffer);
    remote.ProduceResponse(encoder);
    local.ProduceUpdate(encoder, this);
    encoder.Finish();
    return buffer;
}

void NCpeer::ConsumeMessage(const void * data, int size)
{ 
    auto bytes = reinterpret_cast<const uint8_t *>(data);
    std::vector<uint8_t> buffer(bytes, bytes+size);
    ArithmeticDecoder decoder(buffer);
    local.ConsumeResponse(decoder);
    remote.ConsumeUpdate(decoder, this);
}
