#include "netcode.h"

#include "Server.h"
#include "Client.h"

void const * ncGetBlobData(NCblob * blob) { return blob->memory.data(); }
int ncGetBlobSize(NCblob * blob) { return blob->memory.size(); }
void ncFreeBlob(NCblob * blob) { delete blob; }

NCclass * ncCreateClass(int numIntFields) { return new NCclass { numIntFields }; }
NCserver * ncCreateServer(NCclass * const * classes, int numClasses, int maxFrameDelta) { return new NCserver(classes, numClasses, maxFrameDelta); }
NCclient * ncCreateClient(NCclass * const * classes, int numClasses, int maxFrameDelta) { return new NCclient(classes, numClasses, maxFrameDelta); }

NCpeer * ncCreatePeer(NCserver * server) { return server->CreatePeer(); }
NCobject * ncCreateObject(NCserver * server, NCclass * objectClass) {	return server->CreateObject(objectClass); }
void ncPublishFrame(NCserver * server) { return server->PublishFrame(); }

void ncSetVisibility(NCpeer * peer, NCobject * object, int isVisible) { peer->SetVisibility(object, !!isVisible); }
NCblob * ncProduceUpdate(NCpeer * peer) { return new NCblob{peer->ProduceUpdate()}; }
void ncConsumeResponse(NCpeer * peer, const void * data, int size) { peer->ConsumeResponse(reinterpret_cast<const uint8_t *>(data), size); }

void ncSetObjectInt(NCobject * object, int index, int value) { object->SetIntField(index, value); }
void ncDestroyObject(NCobject * object)
{
    object->server->stateAlloc.Free(object->stateOffset, object->cl.sizeBytes);
    for(auto peer : object->server->peers) peer->SetVisibility(object, false);
    auto it = std::find(begin(object->server->objects), end(object->server->objects), object);
    if(it != end(object->server->objects)) object->server->objects.erase(it);
    delete object;
}

void ncConsumeUpdate(NCclient * client, const void * data, int size) { client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(data), size); }
NCblob * ncProduceResponse (NCclient * client) { return new NCblob{client->ProduceResponse()}; }
int ncGetViewCount(NCclient * client) { return client->frames.empty() ? 0 : client->frames.rbegin()->second.views.size(); }

NCview * ncGetView(NCclient * client, int index) {	return client->frames.rbegin()->second.views[index].get(); }
NCclass * ncGetViewClass(NCview * view) { return view->cl.cl; }
int ncGetViewInt(NCview * view, int index) { return view->GetIntField(index); }

template<class A, class B> size_t MemUsage(const std::pair<A,B> & pair) { return MemUsage(pair.first) + MemUsage(pair.second); }
template<class T> size_t MemUsage(const std::shared_ptr<T> & ptr) { return ptr ? MemUsage(ptr.get()) : 0; }
template<class T> size_t MemUsage(const std::vector<T> & vec)
{
    size_t total = vec.capacity() * sizeof(T);
    for(auto & elem : vec) total += MemUsage(elem);
    return total;
}
template<class K, class V> size_t MemUsage(const std::map<K,V> & map)
{
    size_t total = map.size() * sizeof(std::pair<K,V>);
    for(auto & elem : map) total += MemUsage(elem);
    return total;
}

static size_t MemUsage(uint8_t) { return 0; }
static size_t MemUsage(int) { return 0; }
static size_t MemUsage(const NCpeer::ObjectRecord & r) { return 0; }
static size_t MemUsage(const IntegerDistribution & d) { return 0; }
static size_t MemUsage(const Distribs & d) { return MemUsage(d.intFieldDists); }
static size_t MemUsage(const NCclass * cl) { return sizeof(NCclass); }
static size_t MemUsage(const Policy::Field & f) { return 0; }
static size_t MemUsage(const Policy::Class & cl) { return MemUsage(cl.cl) + MemUsage(cl.fields); }
static size_t MemUsage(const Policy & policy) { return MemUsage(policy.classes); }
static size_t MemUsage(const NCobject * obj) { return sizeof(NCobject); }
static size_t MemUsage(const NCpeer * peer) { return sizeof(NCpeer) + MemUsage(peer->records) + MemUsage(peer->visChanges) + MemUsage(peer->frameDistribs); }
static size_t MemUsage(const NCview * peer) { return sizeof(NCview); }
static size_t MemUsage(const ClientFrame & f) { return MemUsage(f.views) + MemUsage(f.state) + MemUsage(f.distribs); }
int ncDebugServerMemoryUsage(NCserver * server)
{
    return sizeof(NCserver) + MemUsage(server->policy) + MemUsage(server->objects) + MemUsage(server->peers) + MemUsage(server->state) + MemUsage(server->frameState);
}
int ncDebugClientMemoryUsage(NCclient * client)
{
    return sizeof(NCclient) + MemUsage(client->policy) + MemUsage(client->frames);
}