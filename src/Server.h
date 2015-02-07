#ifndef NETCODE_SERVER_H
#define NETCODE_SERVER_H

#include "Policy.h"

#include <map>

struct NCpeer;
struct NCobject;

struct NCserver
{
	netcode::Policy policy;
    netcode::RangeAllocator stateAlloc;
	std::vector<NCobject *> objects;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	NCserver(NCclass * const * classes, size_t numClasses, int maxFrameDelta);

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    NCpeer * CreatePeer();
	NCobject * CreateObject(NCclass * objectClass);
    void PublishFrame();
};

struct NCpeer
{
    struct ObjectRecord
    {
        const NCobject * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    NCserver * server;
    std::vector<ObjectRecord> records;
    std::vector<std::pair<const NCobject *,bool>> visChanges;
    std::map<int, netcode::Distribs> frameDistribs;
    std::vector<int> ackFrames;
    int nextId;

    NCpeer(NCserver * server);

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const NCobject * object, bool setVisible);
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size);
};

struct NCobject
{
    NCserver * server;
    const netcode::Policy::Class & cl;
	int stateOffset;

	NCobject(NCserver * server, const netcode::Policy::Class & cl, int stateOffset);

    void SetIntField(int index, int value);
};

#endif