#ifndef SERVER_H
#define SERVER_H

#include "Policy.h"

struct VPeer_;
struct VObject_;

struct VServer_
{
	Policy policy;
    RangeAllocator stateAlloc;
	std::vector<VObject_ *> objects;
    std::vector<VPeer_ *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	VServer_(VClass_ * const * classes, size_t numClasses, int maxFrameDelta);

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    VPeer_ * CreatePeer();
	VObject_ * CreateObject(VClass_ * objectClass);
    void PublishFrame();
};

struct VPeer_
{
    struct ObjectRecord
    {
        const VObject_ * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    VServer_ * server;
    std::vector<ObjectRecord> records;
    std::vector<std::pair<const VObject_ *,bool>> visChanges;
    std::map<int, Distribs> frameDistribs;
    std::vector<int> ackFrames;
    int nextId;

    VPeer_(VServer_ * server);

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const VObject_ * object, bool setVisible);
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size);
};

struct VObject_
{
    VServer_ * server;
    const Policy::Class & cl;
	int stateOffset;

	VObject_(VServer_ * server, const Policy::Class & cl, int stateOffset);

    void SetIntField(int index, int value);
};

#endif