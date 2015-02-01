#ifndef VIEW_INTERNAL_H
#define VIEW_INTERNAL_H

#include "View.h"
#include "Distribution.h"

#include <map>

class RangeAllocator
{
    size_t totalCapacity;
    std::vector<std::pair<size_t,size_t>> freeList;
public:
    RangeAllocator() : totalCapacity() {}

    size_t GetTotalCapacity() const { return totalCapacity; }

    size_t Allocate(size_t amount)
    {
        for(auto it = freeList.rbegin(); it != freeList.rend(); ++it)
        {
            if(it->second == amount)
            {
                auto offset = it->first;
                freeList.erase(freeList.begin() + (&*it - freeList.data()));
                return offset;
            }
        }

        auto offset = totalCapacity;
        totalCapacity += amount;
        return offset;
    }

    void Free(size_t offset, size_t amount)
    {
        freeList.push_back({offset,amount});
    }
};

struct VBlob_
{
    std::vector<uint8_t> memory;
};

struct VClass_
{
	int numIntFields;
};

struct Policy
{
    struct Field { int offset, distIndex; };

    struct Class
    {
        VClass cl;
        int index, sizeBytes;
        std::vector<Field> fields;
    };

    std::vector<Class> classes;
    size_t numIntFields;

    Policy(const VClass * classes, size_t numClasses);
};

struct VObject_
{
    VServer server;
    const Policy::Class & cl;
	int stateOffset;

	VObject_(VServer server, const Policy::Class & cl, int stateOffset);

    void SetIntField(int index, int value);
};

struct VServer_
{
	Policy policy;
    RangeAllocator stateAlloc;
	std::vector<VObject> objects;
    std::vector<VPeer> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	VServer_(const VClass * classes, size_t numClasses);

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    VPeer CreatePeer();
	VObject CreateObject(VClass objectClass);
    void PublishFrame();
};

struct VPeer_
{
    struct ObjectRecord
    {
        const VObject_ * object; int frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
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
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size) {}
};

struct VView_
{
    VClient client;
    const Policy::Class & cl;
    int frameAdded, stateOffset;

	VView_(VClient client, const Policy::Class & cl, int stateOffset, int frameAdded);

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(int index) const;
};

struct VClient_
{
    Policy policy;
    RangeAllocator stateAlloc;
    std::vector<IntegerDistribution> intFieldDists;
    IntegerDistribution newObjectCountDist, delObjectCountDist;

	std::vector<VView> views;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	VClient_(const VClass * classes, size_t numClasses);

    const uint8_t * GetCurrentState() const { return frameState.rbegin()->second.data(); }
    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize);
    std::vector<uint8_t> ProduceResponse();
};

#endif