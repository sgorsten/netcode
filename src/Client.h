#ifndef NETCODE_CLIENT_H
#define NETCODE_CLIENT_H

#include "Protocol.h"

#include <memory>
#include <map>

namespace netcode
{
    struct ClientFrame
    {
        std::vector<std::shared_ptr<NCview>> views;
        std::vector<uint8_t> state;
        Distribs distribs;
    };
}

struct NCclient
{
    const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
    std::map<int, netcode::ClientFrame> frames;
    std::map<int, std::weak_ptr<NCview>> id2View;

	NCclient(const NCprotocol * protocol);

    std::shared_ptr<NCview> CreateView(size_t classIndex, int uniqueId, int frameAdded);

    const uint8_t * GetCurrentState() const { return frames.rbegin()->second.state.data(); }
    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frames.find(frame);
        return it != end(frames) ? it->second.state.data() : nullptr;
    }

	void ConsumeUpdate(const uint8_t * buffer, size_t bufferSize);
    std::vector<uint8_t> ProduceResponse() const;
};

struct NCview
{
    NCclient * client;
    const NCclass * cl;
    int frameAdded, stateOffset;

	NCview(NCclient * client, const NCclass * cl, int stateOffset, int frameAdded);
    ~NCview();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(const NCint * field) const;
};

#endif