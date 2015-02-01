#ifndef VIEW_INTERNAL_H
#define VIEW_INTERNAL_H

#include "View.h"
#include "Distribution.h"

struct VClass_
{
	int numIntFields;
};

struct VObject_
{
    VServer server;
	VClass objectClass;
	std::vector<int> intFields, prevIntFields, prevPrevIntFields;

	VObject_(VServer server, VClass cl);
};

struct VServer_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects;
    std::vector<VPeer> peers;

	VServer_(const VClass * classes, size_t numClasses);

    VPeer CreatePeer();
	VObject CreateObject(VClass objectClass);
};

struct VPeer_
{
    struct ObjectRecord
    {
        VObject object; int frameAdded, frameRemoved; 
        bool isLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    VServer server;
    std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist, delObjectCountDist;

    std::vector<ObjectRecord> records;
    int prevFrame, frame;

    VPeer_(VServer server);

    void SetVisibility(VObject object, bool setVisible);
    std::vector<uint8_t> PublishUpdate();
};

struct VView_
{
	VClass objectClass;
	std::vector<int> intFields, prevIntFields;

	VView_(VClass cl);
};

struct VClient_
{
	std::vector<VClass> classes;
	std::vector<VView> views;
	std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist, delObjectCountDist;

	VClient_(const VClass * classes, size_t numClasses);

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize);
};

#endif