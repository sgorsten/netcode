#ifndef CLIENT_H
#define CLIENT_H

#include "Policy.h"

struct VView_;

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

	VClient_(VClass_ * const classes[], size_t numClasses, int maxFrameDelta);

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

struct VView_
{
    VClient_ * client;
    const Policy::Class & cl;
    int frameAdded, stateOffset;

	VView_(VClient_ * client, const Policy::Class & cl, int stateOffset, int frameAdded);
    ~VView_();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(int index) const;
};

#endif