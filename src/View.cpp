#include "View.h"
#include "Distribution.h"

#include <algorithm>

struct VClass_
{
	int numIntFields;
};

struct VObject_
{
	VClass objectClass;
	std::vector<int> intFields, prevIntFields;

	VObject_(VClass cl) : objectClass(cl), intFields(cl->numIntFields, 0), prevIntFields(intFields) {}
};

struct VServer_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects;
	IntegerDistribution dist;
};

struct VView_
{
	VClass objectClass;
	std::vector<int> intFields;

	VView_(VClass cl) : objectClass(cl), intFields(cl->numIntFields, 0) {}
};

struct VClient_
{
	std::vector<VClass> classes;
	std::vector<VView> views;
	IntegerDistribution dist;
};

VClass vCreateClass(int numIntFields)
{
	return new VClass_ { numIntFields };
}

VServer vCreateServer(const VClass * classes, int numClasses)
{
	auto server = new VServer_;
	server->classes.assign(classes, classes + numClasses);
	return server;
}

VObject vCreateObject(VServer server, VClass objectClass)
{
	// Validate that this is a class the server knows about
	auto it = std::find(begin(server->classes), end(server->classes), objectClass);
	if (it == end(server->classes)) return nullptr;

	// Instantiate the object
	auto object = new VObject_(objectClass);
	server->objects.push_back(object);
	return object;
}

void vSetObjectInt(VObject object, int index, int value)
{
	object->intFields[index] = value;
}

int vPublishUpdate(VServer server, void * buffer, int bufferSize)
{
	std::vector<uint8_t> bytes;
	arith::Encoder encoder(bytes);
	for (auto object : server->objects)
	{
		for (int i = 0; i < object->objectClass->numIntFields; ++i)
		{
			server->dist.EncodeAndTally(encoder, object->intFields[i] - object->prevIntFields[i]);
		}
		object->prevIntFields = object->intFields;
	}
	encoder.Finish();

	memcpy(buffer, bytes.data(), std::min(bytes.size(), size_t(bufferSize)));
	return bytes.size();
}

VClient vCreateClient(const VClass * classes, int numClasses)
{
	auto server = new VClient_;
	server->classes.assign(classes, classes + numClasses);
	return server;
}

VView vCreateView(VClient client, VClass viewClass)
{
	// Validate that this is a class the client knows about
	auto it = std::find(begin(client->classes), end(client->classes), viewClass);
	if (it == end(client->classes)) return nullptr;

	// Instantiate the object
	auto view = new VView_(viewClass);
	client->views.push_back(view);
	return view;
}

void vConsumeUpdate(VClient client, const void * buffer, int bufferSize)
{
	std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(buffer), reinterpret_cast<const uint8_t *>(buffer) +bufferSize);
	arith::Decoder decoder(bytes);
	for (auto view : client->views)
	{
		for (int i = 0; i < view->objectClass->numIntFields; ++i)
		{
			view->intFields[i] += client->dist.DecodeAndTally(decoder);
		}
	}
}

int vGetViewInt(VView object, int index)
{
	return object->intFields[index];
}