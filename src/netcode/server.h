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
#include <set>

namespace netcode
{
    struct Object;
    struct Event;
    struct ObjectView;
    struct EventView;
}

struct NCauthority
{
	const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
	std::vector<netcode::Object *> objects;
    std::vector<netcode::Event *> events;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<netcode::Event *>> eventHistory;
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
    virtual ~NCobject() {}

    virtual void SetIntField(const NCint * field, int value) = 0;
};

namespace netcode
{
    struct Object : public NCobject
    {
        NCauthority * auth;
        const NCclass * cl;
	    int stateOffset;

	    Object(NCauthority * auth, const NCclass * cl, int stateOffset);
        ~Object();

        void SetIntField(const NCint * field, int value) override;
    };

    struct Event : public NCobject
    {
        NCauthority * auth;
        const NCclass * cl;
        std::vector<uint8_t> state;
        bool isPublished;

        Event(NCauthority * auth, const NCclass * cl);
        ~Event();

        void SetIntField(const NCint * field, int value) override;
    };

    struct Client
    {
        struct Frame
        {
            std::vector<std::shared_ptr<netcode::ObjectView>> views;
            std::vector<std::unique_ptr<netcode::EventView>> events;
            std::vector<uint8_t> state;
            Distribs distribs;
        };

        const NCprotocol * protocol;
        netcode::RangeAllocator stateAlloc;
        std::map<int, Frame> frames;
        std::map<int, std::weak_ptr<netcode::ObjectView>> id2View;

	    Client(const NCprotocol * protocol);

        std::shared_ptr<netcode::ObjectView> CreateView(size_t classIndex, int uniqueId, int frameAdded);

        const uint8_t * GetCurrentState() const { return frames.rbegin()->second.state.data(); }
        const uint8_t * GetFrameState(int frame) const
        {
            auto it = frames.find(frame);
            return it != end(frames) ? it->second.state.data() : nullptr;
        }

	    void ConsumeUpdate(netcode::ArithmeticDecoder & decoder);
        void ProduceResponse(netcode::ArithmeticEncoder & encoder) const;
    };
}

struct NCpeer
{
    struct ObjectRecord
    {
        const netcode::Object * object; int uniqueId, frameAdded, frameRemoved; 
        bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
    };

    NCauthority * auth;                                                 // Object authority whose objects may be visible to this peer
    std::vector<ObjectRecord> records;                                  // Records of object visibility
    std::set<const netcode::Event *> visibleEvents;                     // The set of events visible to this peer. Once ncPublishFrame(...) is called, the visibility of all events created that frame is frozen.
    std::vector<std::pair<const netcode::Object *,bool>> visChanges;    // Changes to visibility since the last call to ncPublishFrame(...)
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
    virtual ~NCview() {}

    virtual const NCclass * GetClass() const = 0;
    virtual int GetIntField(const NCint * field) const = 0;
};

namespace netcode
{
    struct ObjectView : public NCview
    {
        netcode::Client * client;
        const NCclass * cl;
        int frameAdded, stateOffset;

	    ObjectView(netcode::Client * client, const NCclass * cl, int stateOffset, int frameAdded);
        ~ObjectView();

        bool IsLive(int frame) const { return frameAdded <= frame; }
        const NCclass * GetClass() const override { return cl; }
        int GetIntField(const NCint * field) const override;
    };

    struct EventView : public NCview
    {
        const NCclass * cl;
        int frameAdded;
        std::vector<uint8_t> state;

	    EventView(const NCclass * cl, int frameAdded, std::vector<uint8_t> state);

        const NCclass * GetClass() const override { return cl; }
        int GetIntField(const NCint * field) const override;
    };
}

#endif