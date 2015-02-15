// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "server.h"
#include "client.h"

struct NCblob { std::vector<uint8_t> memory; };

NCprotocol *    ncCreateProtocol  (int maxFrameDelta)                                       { return new NCprotocol(maxFrameDelta); }
NCclass *       ncCreateClass     (NCprotocol * protocol)                                   { return new NCclass(protocol); }
NCint *         ncCreateInt       (NCclass * cl)                                            { return new NCint(cl); }

NCserver *      ncCreateServer    (const NCprotocol * protocol)                             { return new NCserver(protocol); }

NCpeer *        ncCreatePeer      (NCserver * server)                                       { return server->CreatePeer(); }
NCobject *      ncCreateObject    (NCserver * server, const NCclass * cl)                   { return server->CreateObject(cl); }
void            ncPublishFrame    (NCserver * server)                                       { return server->PublishFrame(); }
void            ncDestroyServer   (NCserver * server)                                       { delete server; }

int             ncGetViewCount    (const NCpeer * peer)                                     { return peer->client.frames.empty() ? 0 : peer->client.frames.rbegin()->second.views.size(); }
const NCview *  ncGetView         (const NCpeer * peer, int index)                          { return peer->client.frames.rbegin()->second.views[index].get(); }
void            ncSetVisibility   (NCpeer * peer, const NCobject * object, int isVisible)   { peer->SetVisibility(object, !!isVisible); }
void            ncDestroyPeer     (NCpeer * peer)                                           { delete peer; }

void            ncSetObjectInt    (NCobject * object, const NCint * field, int value)       { object->SetIntField(field, value); }
void            ncDestroyObject   (NCobject * object)                                       { delete object; }

const NCclass * ncGetViewClass    (const NCview * view)                                     { return view->cl; }
int             ncGetViewInt      (const NCview * view, const NCint * field)                { return view->GetIntField(field); }

const void *    ncGetBlobData     (const NCblob * blob)                                     { return blob->memory.data(); }
int             ncGetBlobSize     (const NCblob * blob)                                     { return blob->memory.size(); }
void            ncFreeBlob        (NCblob * blob)                                           { delete blob; }

NCblob * ncProduceMessage(NCpeer * peer)
{ 
    auto response = peer->client.ProduceResponse();
    auto update = peer->ProduceUpdate();
    auto blob = new NCblob;
    blob->memory.resize(2);
    *reinterpret_cast<uint16_t *>(blob->memory.data()) = response.size();
    blob->memory.insert(end(blob->memory), begin(response), end(response));
    blob->memory.insert(end(blob->memory), begin(update), end(update));
    return blob;
}

void ncConsumeMessage(NCpeer * peer, const void * data, int size)
{ 
    auto bytes = reinterpret_cast<const uint8_t *>(data);
    auto responseSize = *reinterpret_cast<const uint16_t *>(data);
    peer->ConsumeResponse(bytes + 2, responseSize);
    peer->client.ConsumeUpdate(bytes + 2 + responseSize, size - 2 - responseSize);
}