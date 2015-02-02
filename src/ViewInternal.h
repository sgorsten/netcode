#ifndef VIEW_INTERNAL_H
#define VIEW_INTERNAL_H

#include "View.h"
#include "Distribution.h"

#include <memory>
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
    int maxFrameDelta;

    Policy(const VClass * classes, size_t numClasses, int maxFrameDelta);
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

	VServer_(const VClass * classes, size_t numClasses, int maxFrameDelta);

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    VPeer CreatePeer();
	VObject CreateObject(VClass objectClass);
    void PublishFrame();
};

struct Distribs
{
    std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist, delObjectCountDist, uniqueIdDist;
    SymbolDistribution classDist;

    Distribs() {}
    Distribs(const Policy & policy) : intFieldDists(policy.numIntFields), classDist(policy.classes.size()) {}
};

struct VPeer_
{
    struct ObjectRecord
    {
        const VObject_ * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    VServer server;
    std::vector<ObjectRecord> records;
    std::vector<std::pair<const VObject_ *,bool>> visChanges;
    std::map<int, Distribs> frameDistribs;
    std::vector<int> ackFrames;
    int nextId;

    VPeer_(VServer server);

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const VObject_ * object, bool setVisible);
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size);
};

struct VView_
{
    VClient client;
    const Policy::Class & cl;
    int frameAdded, stateOffset;

	VView_(VClient client, const Policy::Class & cl, int stateOffset, int frameAdded);
    ~VView_();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(int index) const;
};

struct ClientFrame
{
    std::vector<std::shared_ptr<VView_>> views;
    std::vector<uint8_t> state;
    Distribs distribs;
};

struct VClient_
{
    Policy policy;
    RangeAllocator stateAlloc;
    std::map<int, ClientFrame> frames;
    std::map<int, std::weak_ptr<VView_>> id2View;

	VClient_(const VClass * classes, size_t numClasses, int maxFrameDelta);

    std::shared_ptr<VView_> CreateView(size_t classIndex, int uniqueId, int frameAdded);

    const uint8_t * GetCurrentState() const { return frames.rbegin()->second.state.data(); }
    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frames.find(frame);
        return it != end(frames) ? it->second.state.data() : nullptr;
    }

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize);
    std::vector<uint8_t> ProduceResponse();
};

#endif