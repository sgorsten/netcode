#include "netcode.h"

#include "Protocol.h"
#include "Server.h"
#include "Client.h"

NCprotocol *    ncCreateProtocol  (int maxFrameDelta)                                       { return new NCprotocol(maxFrameDelta); }
NCclass *       ncCreateClass     (NCprotocol * protocol)                                   { return new NCclass(protocol); }
NCint *         ncCreateInt       (NCclass * cl)                                            { return new NCint(cl); }

NCserver *      ncCreateServer    (const NCprotocol * protocol)                             { return new NCserver(protocol); }
NCclient *      ncCreateClient    (const NCprotocol * protocol)                             { return new NCclient(protocol); }

NCpeer *        ncCreatePeer      (NCserver * server)                                       { return server->CreatePeer(); }
NCobject *      ncCreateObject    (NCserver * server, const NCclass * cl)                   { return server->CreateObject(cl); }
void            ncPublishFrame    (NCserver * server)                                       { return server->PublishFrame(); }

void            ncSetVisibility   (NCpeer * peer, const NCobject * object, int isVisible)   { peer->SetVisibility(object, !!isVisible); }
NCblob *        ncProduceUpdate   (NCpeer * peer)                                           { return new NCblob{peer->ProduceUpdate()}; }
void            ncConsumeResponse (NCpeer * peer, const void * data, int size)              { peer->ConsumeResponse(reinterpret_cast<const uint8_t *>(data), size); }

void            ncSetObjectInt    (NCobject * object, const NCint * field, int value)       { object->SetIntField(field, value); }
void            ncDestroyObject   (NCobject * object)                                       { object->Destroy(); }

int             ncGetViewCount    (const NCclient * client)                                 { return client->frames.empty() ? 0 : client->frames.rbegin()->second.views.size(); }
const NCview *  ncGetView         (const NCclient * client, int index)                      { return client->frames.rbegin()->second.views[index].get(); }
void            ncConsumeUpdate   (NCclient * client, const void * data, int size)          { client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(data), size); }
NCblob *        ncProduceResponse (NCclient * client)                                       { return new NCblob{client->ProduceResponse()}; }

const NCclass * ncGetViewClass    (const NCview * view)                                     { return view->cl; }
int             ncGetViewInt      (const NCview * view, const NCint * field)                { return view->GetIntField(field); }

const void *    ncGetBlobData     (const NCblob * blob)                                     { return blob->memory.data(); }
int             ncGetBlobSize     (const NCblob * blob)                                     { return blob->memory.size(); }
void            ncFreeBlob        (NCblob * blob)                                           { delete blob; }



