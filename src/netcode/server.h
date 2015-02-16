// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#ifndef NETCODE_SERVER_H
#define NETCODE_SERVER_H

#include "protocol.h"

#include <memory>
#include <map>

struct NCauthority
{
	const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
	std::vector<NCobject *> objects;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	NCauthority(const NCprotocol * protocol);
    ~NCauthority();

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    NCpeer * CreatePeer();
	NCobject * CreateObject(const NCclass * objectClass);
    void PublishFrame();
};

struct NCobject
{
    NCauthority * auth;
    const NCclass * cl;
	int stateOffset;

	NCobject(NCauthority * auth, const NCclass * cl, int stateOffset);
    ~NCobject();

    void SetIntField(const NCint * field, int value);
};

namespace netcode
{
    struct Client
    {
        struct Frame
        {
            std::vector<std::shared_ptr<NCview>> views;
            std::vector<uint8_t> state;
            Distribs distribs;
        };

        const NCprotocol * protocol;
        netcode::RangeAllocator stateAlloc;
        std::map<int, Frame> frames;
        std::map<int, std::weak_ptr<NCview>> id2View;

	    Client(const NCprotocol * protocol);

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
}

struct NCpeer
{
    struct ObjectRecord
    {
        const NCobject * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    NCauthority * auth;                                         // Object authority whose objects may be visible to this peer
    std::vector<ObjectRecord> records;                          // Records of object visibility
    std::vector<std::pair<const NCobject *,bool>> visChanges;   // Changes to visibility since the last call to ncPublishFrame(...)
    std::map<int, netcode::Distribs> frameDistribs;             // Probability distributions as they existed at the end of various frames
    std::vector<int> ackFrames;                                 // The set of frames that has been acknowledged by the remote peer
    int nextId;                                                 // The next network ID to use when sending to the remote peer

    netcode::Client client;

    NCpeer(NCauthority * auth);
    ~NCpeer();

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const NCobject * object, bool setVisible);
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size);

    std::vector<uint8_t> ProduceMessage();
    void ConsumeMessage(const void * data, int size);

    int GetViewCount() const { return client.frames.empty() ? 0 : client.frames.rbegin()->second.views.size(); }
    const NCview * GetView(int index) const { return client.frames.rbegin()->second.views[index].get(); }
};

struct NCview
{
    netcode::Client * client;
    const NCclass * cl;
    int frameAdded, stateOffset;

	NCview(netcode::Client * client, const NCclass * cl, int stateOffset, int frameAdded);
    ~NCview();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(const NCint * field) const;
};

#endif