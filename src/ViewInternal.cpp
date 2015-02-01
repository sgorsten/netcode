#include "ViewInternal.h"

#include <algorithm>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value)
{
	return std::find(begin(vec), end(vec), value) - begin(vec);
}

VObject_::VObject_(VServer server, VClass cl) : server(server), objectClass(cl), intFields(cl->numIntFields, 0), prevIntFields(intFields), prevPrevIntFields(intFields) 
{
    
}

VServer_::VServer_(const VClass * classes, size_t numClasses) : classes(classes, classes + numClasses)
{

}

VPeer VServer_::CreatePeer()
{
	auto peer = new VPeer_(this);
	peers.push_back(peer);
	return peer;    
}

VObject VServer_::CreateObject(VClass objectClass)
{
	// Validate that this is a class the server knows about
	auto it = std::find(begin(classes), end(classes), objectClass);
	if (it == end(classes)) return nullptr;

	// Instantiate the object
	auto object = new VObject_(this, objectClass);
	objects.push_back(object);
	return object;
}

VPeer_::VPeer_(VServer server) : server(server)
{
    int numIntFields = 0;
	for (auto cl : server->classes) numIntFields += cl->numIntFields;
	intFieldDists.resize(numIntFields);

    prevFrame = 0;
    frame = 1;
}

void VPeer_::SetVisibility(VObject object, bool setVisible)
{
    for(auto & record : records)
    {
        if(record.object != object) continue;
        if(!record.isLive(frame)) continue;
        if(!setVisible) record.frameRemoved = frame; // Remove object if we were asked to 
        return;                                      // Object is now in the state we wanted
    }

    if(setVisible) // Otherwise, no live record of this object, create one if asked to
    {
        records.push_back({object, frame, INT_MAX});
    }
}

std::vector<uint8_t> VPeer_::PublishUpdate()
{
	std::vector<uint8_t> bytes;
	arith::Encoder encoder(bytes);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<VObject> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.isLive(prevFrame))
        {
            if(!record.isLive(frame)) deletedIndices.push_back(index);  // If it has been removed, store its index
            ++index;                                                    // Either way, object was live, so it used an index
        }
        else if(record.isLive(frame)) // If object was added between last frame and now
        {
            newObjects.push_back(record.object);
        }
    }
    int numPrevObjects = index;
    delObjectCountDist.EncodeAndTally(encoder, deletedIndices.size());
    for(auto index : deletedIndices) EncodeUniform(encoder, index, numPrevObjects);

	// Encode classes of newly created objects
	newObjectCountDist.EncodeAndTally(encoder, newObjects.size());
	for (auto object : newObjects)
	{
		EncodeUniform(encoder, GetIndex(server->classes, object->objectClass), server->classes.size());
	}

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.isLive(frame)) continue;
        auto object = record.object;

		int firstIndex = 0;
		for (auto cl : server->classes)
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

    prevFrame = frame;
    ++frame;

	encoder.Finish();
	return bytes;
}

VView_::VView_(VClass cl) : objectClass(cl), intFields(cl->numIntFields, 0), prevIntFields(intFields) 
{
    
}

VClient_::VClient_(const VClass * classes, size_t numClasses) : classes(classes, classes + numClasses)
{
	int numIntFields = 0;
	for (auto cl : this->classes) numIntFields += cl->numIntFields;
	intFieldDists.resize(numIntFields);
}

void VClient_::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
	std::vector<uint8_t> bytes(buffer, buffer + bufferSize);
	arith::Decoder decoder(bytes);

    // Decode indices of deleted objects
    int delObjects = delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, views.size());
        delete views[index];
        views[index] = 0;
    }
    views.erase(remove_if(begin(views), end(views), [](VView v) { return !v; }), end(views));

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