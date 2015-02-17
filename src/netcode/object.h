// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#ifndef NETCODE_OBJECT_H
#define NETCODE_OBJECT_H

#include "protocol.h"

#include <memory>
#include <map>
#include <set>

namespace netcode { struct Client; }

struct NCauthority
{
	const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
	std::vector<NCobject *> objects;
    std::vector<NCobject *> events;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<NCobject *>> eventHistory;
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
    std::vector<uint8_t> constState;
	int varStateOffset;
    bool isPublished;

	NCobject(NCauthority * auth, const NCclass * cl);

    void Destroy();
    void SetIntField(const NCint * field, int value);
};

struct netcode::Client
{
    struct Frame
    {
        std::vector<std::shared_ptr<NCview>> views;
        std::vector<std::unique_ptr<NCview>> events;
        Distribs distribs;
    };

    const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
    std::map<int, Frame> frames;
    std::map<int, std::vector<uint8_t>> frameStates;
    std::map<int, std::weak_ptr<NCview>> id2View;

	Client(const NCprotocol * protocol);

    std::shared_ptr<NCview> CreateView(size_t classIndex, int uniqueId, int frameAdded, std::vector<uint8_t> constState);

    const uint8_t * GetCurrentState() const { return frameStates.rbegin()->second.data(); }
    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameStates.find(frame);
        return it != end(frameStates) ? it->second.data() : nullptr;
    }

	void ConsumeUpdate(netcode::ArithmeticDecoder & decoder);
    void ProduceResponse(netcode::ArithmeticEncoder & encoder) const;
};

struct NCpeer
{
    struct ObjectRecord
    {
        const NCobject * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    NCauthority * auth;                                                 // Object authority whose objects may be visible to this peer
    std::vector<ObjectRecord> records;                                  // Records of object visibility
    std::set<const NCobject *> visibleEvents;                           // The set of events visible to this peer. Once ncPublishFrame(...) is called, the visibility of all events created that frame is frozen.
    std::vector<std::pair<const NCobject *,bool>> visChanges;           // Changes to visibility of objects (not events) since the last call to ncPublishFrame(...)
    std::map<int, netcode::Distribs> frameDistribs;                     // Probability distributions as they existed at the end of various frames
    std::vector<int> ackFrames;                                         // The set of frames that has been acknowledged by the remote peer
    int nextId;                                                         // The next network ID to use when sending to the remote peer

    netcode::Client client;

    NCpeer(NCauthority * auth);
    ~NCpeer();

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const NCobject * object, bool setVisible);

    void ProduceUpdate(netcode::ArithmeticEncoder & encoder);
    void ConsumeResponse(netcode::ArithmeticDecoder & decoder);

    std::vector<uint8_t> ProduceMessage();
    void ConsumeMessage(const void * data, int size);

    int GetViewCount() const;
    const NCview * GetView(int index) const;
};

struct NCview
{
    netcode::Client * client;
    const NCclass * cl;
    int frameAdded;
    std::vector<uint8_t> constState;
	int varStateOffset;
    
	NCview(netcode::Client * client, const NCclass * cl, int frameAdded, std::vector<uint8_t> constState);
    ~NCview();

    bool IsLive(int frame) const { return frameAdded <= frame; }
    int GetIntField(const NCint * field) const;
};

#endif