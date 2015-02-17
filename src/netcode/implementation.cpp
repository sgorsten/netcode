// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "implementation.h"

struct NCblob { std::vector<uint8_t> memory; };

NCprotocol *     ncCreateProtocol       (int maxFrameDelta)                                     { return new NCprotocol(maxFrameDelta); }
NCclass *        ncCreateClass          (NCprotocol * protocol, int flags)                      { return new NCclass(protocol, flags & NC_EVENT_CLASS_FLAG); }
NCint *          ncCreateInt            (NCclass * cl, int flags)                               { return (cl->isEvent && !(flags & NC_CONST_FIELD_FLAG)) ? nullptr : new NCint(cl, flags); }
NCref *          ncCreateRef            (NCclass * cl)                                          { return cl->isEvent ? nullptr : new NCref(cl); }                               
NCauthority *    ncCreateAuthority      (const NCprotocol * protocol)                           { return new NCauthority(protocol); }
                                        
NCpeer *         ncCreatePeer           (NCauthority * authority)                               { return authority->CreatePeer(); }
NCobject *       ncCreateLocalObject    (NCauthority * authority, const NCclass * cl)           { return authority->CreateObject(cl); }
void             ncPublishFrame         (NCauthority * authority)                               { return authority->PublishFrame(); }
void             ncDestroyAuthority     (NCauthority * authority)                               { delete authority; }
                                        
const NCclass *  ncGetObjectClass       (const NCobject * object)                               { return object->GetClass(); }
int              ncGetObjectInt         (const NCobject * object, const NCint * field)          { return object->GetInt(field); }
const NCobject * ncGetObjectRef         (const NCobject * object, const NCref * field)          { return object->GetRef(field); }
void             ncSetObjectInt         (NCobject * o, const NCint * f, int value)              { o->SetInt(f, value); }
void             ncSetObjectRef         (NCobject * o, const NCref * f, const NCobject * value) { o->SetRef(f, value); }
void             ncDestroyObject        (NCobject * object)                                     { object->Destroy(); }

int              ncGetRemoteObjectCount (const NCpeer * peer)                                   { return peer->remote.GetObjectCount(); }
const NCobject * ncGetRemoteObject      (const NCpeer * peer, int index)                        { return peer->remote.GetObjectFromIndex(index); }
void             ncSetVisibility        (NCpeer * peer, const NCobject * object, int isVisible) { object->SetVisibility(peer, !!isVisible); }
NCblob *         ncProduceMessage       (NCpeer * peer)                                         { return new NCblob{peer->ProduceMessage()}; }
void             ncConsumeMessage       (NCpeer * peer, const void * data, int size)            { peer->ConsumeMessage(data, size); }
void             ncDestroyPeer          (NCpeer * peer)                                         { delete peer; }
                                                    
const void *     ncGetBlobData          (const NCblob * blob)                                   { return blob->memory.data(); }
int              ncGetBlobSize          (const NCblob * blob)                                   { return blob->memory.size(); }
void             ncFreeBlob             (NCblob * blob)                                         { delete blob; }