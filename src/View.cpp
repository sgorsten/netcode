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
	bool isServerObject;
	std::vector<int> intFields, prevIntFields;

	VObject_(VClass cl, bool isServerObject) : objectClass(cl), isServerObject(isServerObject), intFields(cl->numIntFields, 0), prevIntFields(intFields) {}
};

struct VServer_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects;
	IntegerDistribution dist;
};

struct VClient_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects;
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

VObject vCreateServerObject(VServer server, VClass objectClass)
{
	// Validate that this is a class the server knows about
	auto it = std::find(begin(server->classes), end(server->classes), objectClass);
	if (it == end(server->classes)) return nullptr;

	// Instantiate the object
	auto object = new VObject_(objectClass, true);
	server->objects.push_back(object);
	return object;
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

VObject vCreateClientObject(VClient client, VClass objectClass)
{
	// Validate that this is a class the client knows about
	auto it = std::find(begin(client->classes), end(client->classes), objectClass);
	if (it == end(client->classes)) return nullptr;

	// Instantiate the object
	auto object = new VObject_(objectClass, false);
	client->objects.push_back(object);
	return object;
}

void vConsumeUpdate(VClient client, const void * buffer, int bufferSize)
{
	std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(buffer), reinterpret_cast<const uint8_t *>(buffer) +bufferSize);
	arith::Decoder decoder(bytes);
	for (auto object : client->objects)
	{
		for (int i = 0; i < object->objectClass->numIntFields; ++i)
		{
			object->prevIntFields[i] = object->intFields[i];
			object->intFields[i] = object->prevIntFields[i] + client->dist.DecodeAndTally(decoder);
		}
	}
}

void vSetObjectState(VObject object, const int * intFields)
{
	if (!object->isServerObject) return; // Not permitted to modify client objects

	object->intFields.assign(intFields, intFields + object->intFields.size());
}

void vGetObjectState(VObject object, int * intFields)
{
	std::copy(begin(object->intFields), end(object->intFields), intFields);
}