// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "server.h"

struct NCblob { std::vector<uint8_t> memory; };

NCprotocol *    ncCreateProtocol  (int maxFrameDelta)                                       { return new NCprotocol(maxFrameDelta); }
NCclass *       ncCreateClass     (NCprotocol * protocol)                                   { return new NCclass(protocol); }
NCint *         ncCreateInt       (NCclass * cl)                                            { return new NCint(cl); }

NCauthority *   ncCreateAuthority (const NCprotocol * protocol)                             { return new NCauthority(protocol); }

NCpeer *        ncCreatePeer      (NCauthority * authority)                                 { return authority->CreatePeer(); }
NCobject *      ncCreateObject    (NCauthority * authority, const NCclass * cl)             { return authority->CreateObject(cl); }
void            ncPublishFrame    (NCauthority * authority)                                 { return authority->PublishFrame(); }
void            ncDestroyAuthority(NCauthority * authority)                                 { delete authority; }

int             ncGetViewCount    (const NCpeer * peer)                                     { return peer->GetViewCount(); }
const NCview *  ncGetView         (const NCpeer * peer, int index)                          { return peer->GetView(index); }
void            ncSetVisibility   (NCpeer * peer, const NCobject * object, int isVisible)   { peer->SetVisibility(object, !!isVisible); }
NCblob *        ncProduceMessage  (NCpeer * peer)                                           { return new NCblob{peer->ProduceMessage()}; }
void            ncConsumeMessage  (NCpeer * peer, const void * data, int size)              { peer->ConsumeMessage(data, size); }
void            ncDestroyPeer     (NCpeer * peer)                                           { delete peer; }

void            ncSetObjectInt    (NCobject * object, const NCint * field, int value)       { object->SetIntField(field, value); }
void            ncDestroyObject   (NCobject * object)                                       { delete object; }

const NCclass * ncGetViewClass    (const NCview * view)                                     { return view->cl; }
int             ncGetViewInt      (const NCview * view, const NCint * field)                { return view->GetIntField(field); }

const void *    ncGetBlobData     (const NCblob * blob)                                     { return blob->memory.data(); }
int             ncGetBlobSize     (const NCblob * blob)                                     { return blob->memory.size(); }
void            ncFreeBlob        (NCblob * blob)                                           { delete blob; }