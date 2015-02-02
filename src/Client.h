#ifndef CLIENT_H
#define CLIENT_H

#include "Policy.h"

#include <memory>
#include <map>

struct NCview;

struct ClientFrame
{
    std::vector<std::shared_ptr<NCview>> views;
    std::vector<uint8_t> state;
    Distribs distribs;
};

struct NCclient
{
    Policy policy;
    RangeAllocator stateAlloc;
    std::map<int, ClientFrame> frames;
    std::map<int, std::weak_ptr<NCview>> id2View;

	NCclient(NCclass * const classes[], size_t numClasses, int maxFrameDelta);

    std::shared_ptr<NCview> CreateView(size_t classIndex, int uniqueId, int frameAdded);

    const uint8_t * GetCurrentState() const { return frames.rbegin()->second.state.data(); }
    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frames.find(frame);
        return it != end(frames) ? it->second.state.data() : nullptr;
    }

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize);
    std::vector<uint8_t> ProduceResponse();
};

struct NCview
{
    NCclient * client;
    const Policy::Class & cl;
    int frameAdded, stateOffset;

	NCview(NCclient * client, const Policy::Class & cl, int stateOffset, int frameAdded);
    ~NCview();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(int index) const;
};

#endif