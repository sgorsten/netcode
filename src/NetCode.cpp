#include "netcode.h"

#include "Server.h"
#include "Client.h"

void const * ncGetBlobData(NCblob * blob) { return blob->memory.data(); }
int ncGetBlobSize(NCblob * blob) { return blob->memory.size(); }
void ncFreeBlob(NCblob * blob) { delete blob; }

NCprotocol * ncCreateProtocol(int maxFrameDelta) { return new NCprotocol(maxFrameDelta); }
NCclass * ncCreateClass(NCprotocol * protocol) { return new NCclass(protocol); }
NCint * ncCreateInt(NCclass * class_) { return new NCint(class_); }

NCserver * ncCreateServer(NCprotocol * protocol) { return new NCserver(protocol); }
NCclient * ncCreateClient(NCprotocol * protocol) { return new NCclient(protocol); }

NCpeer * ncCreatePeer(NCserver * server) { return server->CreatePeer(); }
NCobject * ncCreateObject(NCserver * server, NCclass * objectClass) {	return server->CreateObject(objectClass); }
void ncPublishFrame(NCserver * server) { return server->PublishFrame(); }

void ncSetVisibility(NCpeer * peer, NCobject * object, int isVisible) { peer->SetVisibility(object, !!isVisible); }
NCblob * ncProduceUpdate(NCpeer * peer) { return new NCblob{peer->ProduceUpdate()}; }
void ncConsumeResponse(NCpeer * peer, const void * data, int size) { peer->ConsumeResponse(reinterpret_cast<const uint8_t *>(data), size); }

void ncSetObjectInt(NCobject * object, NCint * field, int value) { object->SetIntField(field, value); }
void ncDestroyObject(NCobject * object)
{
    object->server->stateAlloc.Free(object->stateOffset, object->cl->sizeInBytes);
    for(auto peer : object->server->peers) peer->SetVisibility(object, false);
    auto it = std::find(begin(object->server->objects), end(object->server->objects), object);
    if(it != end(object->server->objects)) object->server->objects.erase(it);
    delete object;
}

void ncConsumeUpdate(NCclient * client, const void * data, int size) { client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(data), size); }
NCblob * ncProduceResponse (NCclient * client) { return new NCblob{client->ProduceResponse()}; }
int ncGetViewCount(NCclient * client) { return client->frames.empty() ? 0 : client->frames.rbegin()->second.views.size(); }

NCview * ncGetView(NCclient * client, int index) {	return client->frames.rbegin()->second.views[index].get(); }
NCclass * ncGetViewClass(NCview * view) { return view->cl; }
int ncGetViewInt(NCview * view, NCint * field) { return view->GetIntField(field); }
