#include "ViewInternal.h"

#include <algorithm>

VClass vCreateClass(int numIntFields) { return new VClass_ { numIntFields }; }
VServer vCreateServer(const VClass * classes, int numClasses) { return new VServer_(classes, numClasses); }
VClient vCreateClient(const VClass * classes, int numClasses) {	return new VClient_(classes, numClasses); }

VPeer vCreatePeer(VServer server) { return server->CreatePeer(); }
VObject vCreateObject(VServer server, VClass objectClass) {	return server->CreateObject(objectClass); }

void vSetVisibility(VPeer peer, VObject object, int isVisible) { peer->SetVisibility(object, !!isVisible); }
int vPublishUpdate(VPeer peer, void * buffer, int bufferSize)
{
	auto bytes = peer->PublishUpdate();
	memcpy(buffer, bytes.data(), std::min(bytes.size(), size_t(bufferSize)));
	return bytes.size();
}

void vSetObjectInt(VObject object, int index, int value) { object->intFields[index] = value; }
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