#include "ViewInternal.h"

#include <algorithm>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value)
{
	return std::find(begin(vec), end(vec), value) - begin(vec);
}

VObject_::VObject_(VServer server, const Class & cl) : server(server), cl(cl), state(cl.sizeBytes, 0)
{
    
}

VServer_::VServer_(const VClass * classes, size_t numClasses) : numIntDistributions(), frame()
{
    for(size_t i=0; i<numClasses; ++i)
    {
        Class cl;
        cl.cl = classes[i];
        cl.index = i;
        cl.sizeBytes = 0;
        for(int fieldIndex = 0; fieldIndex < classes[i]->numIntFields; ++fieldIndex)
        {
            cl.fields.push_back({cl.sizeBytes, numIntDistributions++});
            cl.sizeBytes += sizeof(int);
        }
        this->classes.push_back(cl);
    }
}

VPeer VServer_::CreatePeer()
{
	auto peer = new VPeer_(this);
	peers.push_back(peer);
	return peer;    
}

VObject VServer_::CreateObject(VClass objectClass)
{
    for(auto & cl : classes)
    {
        if(cl.cl == objectClass)
        {
	        auto object = new VObject_(this, cl);
	        objects.push_back(object);
	        return object;
        }
    }
    return nullptr;
}

void VServer_::PublishFrame()
{
    ++frame;
    for(auto object : objects) object->OnPublishFrame(frame);
    for(auto peer : peers) peer->OnPublishFrame(frame);

    // Expire old frames
    for(auto object : objects) object->frameState.erase(frame - 3);
}

VPeer_::VPeer_(VServer server) : server(server)
{
    intFieldDists.resize(server->numIntDistributions);
    prevFrame = 0;
    frame = 1;
}

void VPeer_::OnPublishFrame(int frame)
{
    for(auto change : visChanges)
    {
        auto it = std::find_if(begin(records), end(records), [=](ObjectRecord & r) { return r.object == change.first && r.isLive(frame); });
        if((it != end(records)) == change.second) continue; // If object visibility is as desired, skip this change
        if(change.second) records.push_back({change.first, frame, INT_MAX}); // Make object visible
        else it->frameRemoved = frame; // Make object invisible
    }
    visChanges.clear();
    this->frame = frame;

    records.erase(remove_if(begin(records), end(records), [=](ObjectRecord & r) { return r.frameRemoved < frame-2; }), end(records));
}

void VPeer_::SetVisibility(const VObject_ * object, bool setVisible)
{
    visChanges.push_back({object,setVisible});
}

std::vector<uint8_t> VPeer_::PublishUpdate()
{
	std::vector<uint8_t> bytes;
	arith::Encoder encoder(bytes);

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const VObject_ *> newObjects;
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
	for (auto object : newObjects) EncodeUniform(encoder, object->cl.index, server->classes.size());

	// Encode updates for each view
    for(const auto & record : records)
	{
        if(!record.isLive(frame)) continue;
        auto object = record.object;

        for(auto & field : object->cl.fields)
		{
            int value = record.GetIntField(frame, field.offset);
            int prevValue = record.GetIntField(frame-1, field.offset);
            int prevPrevValue = record.GetIntField(frame-2, field.offset);
			intFieldDists[field.distIndex].EncodeAndTally(encoder, value - prevValue * 2 + prevPrevValue);
		}
	}

    prevFrame = frame;

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