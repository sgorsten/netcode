#ifndef VIEW_INTERNAL_H
#define VIEW_INTERNAL_H

#include "View.h"
#include "Distribution.h"

#include <map>

struct VClass_
{
	int numIntFields;
};

struct Field
{
    int offset, distIndex;
};

struct Class
{
    VClass cl;
    int index, sizeBytes;
    std::vector<Field> fields;
};

struct VObject_
{
    VServer server;
    const Class & cl;
	int stateOffset;

	VObject_(VServer server, const Class & cl, int stateOffset);

    void SetIntField(int index, int value);
    int GetIntField(int frame, int offset) const;
};

struct VServer_
{
	std::vector<Class> classes;
    size_t numIntDistributions;

	std::vector<VObject> objects;
    std::vector<VPeer> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
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
        int GetIntField(int frame, int offset) const { return isLive(frame) ? object->GetIntField(frame, offset) : 0; }
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