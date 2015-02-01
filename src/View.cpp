#include "ViewInternal.h"

#include <algorithm>

VClass vCreateClass(int numIntFields) { return new VClass_ { numIntFields }; }
VServer vCreateServer(const VClass * classes, int numClasses) { return new VServer_(classes, numClasses); }
VClient vCreateClient(const VClass * classes, int numClasses) {	return new VClient_(classes, numClasses); }

VPeer vCreatePeer(VServer server) { return server->CreatePeer(); }
VObject vCreateObject(VServer server, VClass objectClass) {	return server->CreateObject(objectClass); }
void vPublishFrame(VServer server) { return server->PublishFrame(); }

void vSetVisibility(VPeer peer, VObject object, int isVisible) { peer->SetVisibility(object, !!isVisible); }
int vPublishUpdate(VPeer peer, void * buffer, int bufferSize)
{
	auto bytes = peer->PublishUpdate();
	memcpy(buffer, bytes.data(), std::min(bytes.size(), size_t(bufferSize)));
	return bytes.size();
}

void vSetObjectInt(VObject object, int index, int value) { object->SetIntField(index, value); }
void vDestroyObject(VObject object)
{
    for(auto peer : object->server->peers) peer->SetVisibility(object, false);
    auto it = std::find(begin(object->server->objects), end(object->server->objects), object);
    if(it != end(object->server->objects)) object->server->objects.erase(it);
    delete object;
}

void vConsumeUpdate(VClient client, const void * buffer, int bufferSize) { client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(buffer), bufferSize); }
int vGetViewCount(VClient client) { return client->views.size(); }

VView vGetView(VClient client, int index) {	return client->views[index];}
VClass vGetViewClass(VView view) { return view->objectClass; }
int vGetViewInt(VView view, int index) { return view->intFields[index]; }

template<class A, class B> size_t MemUsage(const std::pair<A,B> & pair) { return MemUsage(pair.first) + MemUsage(pair.second); }
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
static size_t MemUsage(const VClass_ * cl) { return sizeof(VClass_); }
static size_t MemUsage(const Class::Field & f) { return 0; }
static size_t MemUsage(const Class & cl) { return MemUsage(cl.cl) + MemUsage(cl.fields); }
static size_t MemUsage(const VObject_ * obj) { return sizeof(VObject_); }
static size_t MemUsage(const VPeer_ * peer) { return sizeof(VPeer_) + MemUsage(peer->records) + MemUsage(peer->visChanges); }
int vDebugServerMemoryUsage(VServer server)
{
    return sizeof(VServer_) + MemUsage(server->classes) + MemUsage(server->objects) + MemUsage(server->peers) + MemUsage(server->state) + MemUsage(server->frameState);
}