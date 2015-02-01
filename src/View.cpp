#include "View.h"
#include "Distribution.h"

#include <algorithm>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value)
{
	return std::find(begin(vec), end(vec), value) - begin(vec);
}

struct VClass_
{
	int numIntFields;
};

struct VObject_
{
	VClass objectClass;
	std::vector<int> intFields, prevIntFields, prevPrevIntFields;

	VObject_(VClass cl) : objectClass(cl), intFields(cl->numIntFields, 0), prevIntFields(intFields), prevPrevIntFields(intFields) {}
};

struct VServer_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects, prevObjects;
	std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist;

	VServer_(const VClass * classes, size_t numClasses) : classes(classes, classes + numClasses)
	{
		int numIntFields = 0;
		for (auto cl : this->classes) numIntFields += cl->numIntFields;
		intFieldDists.resize(numIntFields);
	}

	VObject CreateObject(VClass objectClass)
	{
		// Validate that this is a class the server knows about
		auto it = std::find(begin(classes), end(classes), objectClass);
		if (it == end(classes)) return nullptr;

		// Instantiate the object
		auto object = new VObject_(objectClass);
		objects.push_back(object);
		return object;
	}

	std::vector<uint8_t> PublishUpdate()
	{
		std::vector<uint8_t> bytes;
		arith::Encoder encoder(bytes);

		// Encode classes of newly created objects
		newObjectCountDist.EncodeAndTally(encoder, objects.size() - prevObjects.size());
		for (size_t i = prevObjects.size(); i < objects.size(); ++i)
		{
			EncodeUniform(encoder, GetIndex(classes, objects[i]->objectClass), classes.size());
		}
		prevObjects = objects;

		// Encode updates for each view
		for (auto object : objects)
		{
			int firstIndex = 0;
			for (auto cl : classes)
			{
				if (cl == object->objectClass) break;
				firstIndex += cl->numIntFields;
			}
			for (int i = 0; i < object->objectClass->numIntFields; ++i)
			{
				intFieldDists[firstIndex + i].EncodeAndTally(encoder, object->intFields[i] - object->prevIntFields[i] * 2 + object->prevPrevIntFields[i]);
			}
			object->prevPrevIntFields = object->prevIntFields;
			object->prevIntFields = object->intFields;
		}

		encoder.Finish();
		return bytes;
	}
};

struct VView_
{
	VClass objectClass;
	std::vector<int> intFields, prevIntFields;

	VView_(VClass cl) : objectClass(cl), intFields(cl->numIntFields, 0), prevIntFields(intFields) {}
};

struct VClient_
{
	std::vector<VClass> classes;
	std::vector<VView> views;
	std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist;

	VClient_(const VClass * classes, size_t numClasses) : classes(classes, classes + numClasses)
	{
		int numIntFields = 0;
		for (auto cl : this->classes) numIntFields += cl->numIntFields;
		intFieldDists.resize(numIntFields);
	}

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
	{
		std::vector<uint8_t> bytes(buffer, buffer + bufferSize);
		arith::Decoder decoder(bytes);

		// Decode classes of newly created objects, and instantiate corresponding views
		int newObjects = newObjectCountDist.DecodeAndTally(decoder);
		for (int i = 0; i < newObjects; ++i)
		{
			views.push_back(new VView_(classes[DecodeUniform(decoder, classes.size())]));
		}

		// Decode updates for each view
		for (auto view : views)
		{
			int firstIndex = 0;
			for (auto cl : classes)
			{
				if (cl == view->objectClass) break;
				firstIndex += cl->numIntFields;
			}
			for (int i = 0; i < view->objectClass->numIntFields; ++i)
			{
				auto prev = view->intFields[i];
				auto delta = prev - view->prevIntFields[i];
				view->intFields[i] += delta + intFieldDists[firstIndex + i].DecodeAndTally(decoder);
				view->prevIntFields[i] = prev;
			}
		}
	}
};

////////////////////////
// API implementation //
////////////////////////

VClass vCreateClass(int numIntFields)
{
	return new VClass_ { numIntFields };
}

VServer vCreateServer(const VClass * classes, int numClasses)
{
	return new VServer_(classes, numClasses);
}

VObject vCreateObject(VServer server, VClass objectClass)
{
	return server->CreateObject(objectClass);
}

void vSetObjectInt(VObject object, int index, int value)
{
	object->intFields[index] = value;
}

int vPublishUpdate(VServer server, void * buffer, int bufferSize)
{
	auto bytes = server->PublishUpdate();
	memcpy(buffer, bytes.data(), std::min(bytes.size(), size_t(bufferSize)));
	return bytes.size();
}

VClient vCreateClient(const VClass * classes, int numClasses)
{
	return new VClient_(classes, numClasses);
}

void vConsumeUpdate(VClient client, const void * buffer, int bufferSize)
{
	client->ConsumeUpdate(reinterpret_cast<const uint8_t *>(buffer), bufferSize);
}

int vGetViewCount(VClient client)
{
	return client->views.size();
}

VView vGetView(VClient client, int index)
{
	return client->views[index];
}

VClass vGetViewClass(VView view)
{
	return view->objectClass;
}

int vGetViewInt(VView view, int index)
{
	return view->intFields[index];
}