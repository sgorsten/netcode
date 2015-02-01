#ifndef VIEW_INTERNAL_H
#define VIEW_INTERNAL_H

#include "View.h"
#include "Distribution.h"

#include <map>

struct VClass_
{
	int numIntFields;
};

struct VObject_
{
    VServer server;
	VClass objectClass;
	std::vector<int> intFields;
    std::map<int, std::vector<int>> frameIntFields;

	VObject_(VServer server, VClass cl);

    int GetIntField(int frame, int index) const
    {
        auto it = frameIntFields.find(frame);
        return it == end(frameIntFields) ? 0 : it->second[index];
    }
};

struct VServer_
{
	std::vector<VClass> classes;
	std::vector<VObject> objects;
    std::vector<VPeer> peers;
    int frame;

	VServer_(const VClass * classes, size_t numClasses);

    VPeer CreatePeer();
	VObject CreateObject(VClass objectClass);
    void PublishFrame();
};

struct VPeer_
{
    struct ObjectRecord
    {
        const VObject_ * object; int frameAdded, frameRemoved; 
        bool isLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
        int GetIntField(int frame, int index) const { return isLive(frame) ? object->GetIntField(frame, index) : 0; }
    };

    VServer server;
    std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist, delObjectCountDist;

    std::vector<ObjectRecord> records;
    std::vector<std::pair<const VObject_ *,bool>> visChanges;
    int prevFrame, frame;

    VPeer_(VServer server);

    void OnPublishFrame(int frame);
    void SetVisibility(const VObject_ * object, bool setVisible);
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