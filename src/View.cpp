#include "View.h"
#include "Server.h"
#include "Client.h"

void * vGetBlobData(VBlob blob) { return blob->memory.data(); }
int vGetBlobSize(VBlob blob) { return blob->memory.size(); }
void vFreeBlob(VBlob blob) { delete blob; }

VClass vCreateClass(int numIntFields) { return new VClass_ { numIntFields }; }
VServer vCreateServer(const VClass * classes, int numClasses, int maxFrameDelta) { return new VServer_(classes, numClasses, maxFrameDelta); }
VClient vCreateClient(const VClass * classes, int numClasses, int maxFrameDelta) { return new VClient_(classes, numClasses, maxFrameDelta); }

VPeer vCreatePeer(VServer server) { return server->CreatePeer(); }
VObject vCreateObject(VServer server, VClass objectClass) {	return server->CreateObject(objectClass); }
void vPublishFrame(VServer server) { return server->PublishFrame(); }

void vSetVisibility(VPeer peer, VObject object, int isVisible) { peer->SetVisibility(object, !!isVisible); }
VBlob vProduceUpdate(VPeer peer) { return new VBlob_{peer->ProduceUpdate()}; }
void vConsumeResponse(VPeer peer, const void * data, int size) { peer->ConsumeResponse(reinterpret_cast<const uint8_t *>(data), size); }

void vSetObjectInt(VObject object, int index, int value) { object->SetIntField(index, value); }
void vDestroyObject(VObject object)
{
    object->server->stateAlloc.Free(object->stateOffset, object->cl.sizeBytes);
    for(auto peer : object->server->peers) peer->SetVisibility(object, false);
    auto it = std::find(begin(object->server->objects), end(object->server->objects), object);
    if(it != end(object->server->objects)) object->server->objects.erase(it);
    delete object;
}

void vConsumeUpdate(VClient client, const void * data, int size) { client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(data), size); }
VBlob vProduceResponse (VClient client) { return new VBlob_{client->ProduceResponse()}; }
int vGetViewCount(VClient client) { return client->frames.empty() ? 0 : client->frames.rbegin()->second.views.size(); }

VView vGetView(VClient client, int index) {	return client->frames.rbegin()->second.views[index].get(); }
VClass vGetViewClass(VView view) { return view->cl.cl; }
int vGetViewInt(VView view, int index) { return view->GetIntField(index); }

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
static size_t MemUsage(const VPeer_::ObjectRecord & r) { return 0; }
static size_t MemUsage(const IntegerDistribution & d) { return 0; }
static size_t MemUsage(const Distribs & d) { return MemUsage(d.intFieldDists); }
static size_t MemUsage(const VClass_ * cl) { return sizeof(VClass_); }
static size_t MemUsage(const Policy::Field & f) { return 0; }
static size_t MemUsage(const Policy::Class & cl) { return MemUsage(cl.cl) + MemUsage(cl.fields); }
static size_t MemUsage(const Policy & policy) { return MemUsage(policy.classes); }
static size_t MemUsage(const VObject_ * obj) { return sizeof(VObject_); }
static size_t MemUsage(const VPeer_ * peer) { return sizeof(VPeer_) + MemUsage(peer->records) + MemUsage(peer->visChanges) + MemUsage(peer->frameDistribs); }
static size_t MemUsage(const VView_ * peer) { return sizeof(VView_); }
static size_t MemUsage(const ClientFrame & f) { return MemUsage(f.views) + MemUsage(f.state) + MemUsage(f.distribs); }
int vDebugServerMemoryUsage(VServer server)
{
    return sizeof(VServer_) + MemUsage(server->policy) + MemUsage(server->objects) + MemUsage(server->peers) + MemUsage(server->state) + MemUsage(server->frameState);
}
int vDebugClientMemoryUsage(VClient client)
{
    return sizeof(VClient_) + MemUsage(client->policy) + MemUsage(client->frames);
}