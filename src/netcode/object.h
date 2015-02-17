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

namespace netcode 
{ 
    struct LocalObject;

    class LocalSet
    {
        struct Record;

        const NCauthority * auth;                                           // Object authority whose objects may be visible to this peer
        std::vector<Record> records;                                  // Records of object visibility
        std::set<const LocalObject *> visibleEvents;                           // The set of events visible to this peer. Once ncPublishFrame(...) is called, the visibility of all events created that frame is frozen.
        std::vector<std::pair<const LocalObject *,bool>> visChanges;           // Changes to visibility of objects (not events) since the last call to ncPublishFrame(...)
        std::map<int, Distribs> frameDistribs;                     // Probability distributions as they existed at the end of various frames
        std::vector<int> ackFrames;                                         // The set of frames that has been acknowledged by the remote peer
        int nextId;                                                         // The next network ID to use when sending to the remote peer
    public:
        LocalSet(const NCauthority * auth);
        ~LocalSet();

        const NCobject * GetObjectFromUniqueId(int uniqueId) const;
        int GetUniqueIdFromObject(const NCobject * object, int frame) const;

        int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
        void OnPublishFrame(int frame);
        void SetVisibility(const LocalObject * object, bool setVisible);

        void ProduceUpdate(ArithmeticEncoder & encoder, NCpeer * peer);
        void ConsumeResponse(ArithmeticDecoder & decoder);    
        void PurgeReferences();
    };

    class RemoteSet
    {
        struct Object;
        struct Frame;

        const NCprotocol * protocol;
        netcode::RangeAllocator stateAlloc;
        std::map<int, Frame> frames;
        std::map<int, std::vector<uint8_t>> frameStates;
        std::map<int, std::weak_ptr<Object>> id2View;
        std::vector<std::unique_ptr<Object>> events;
    public:
	    RemoteSet(const NCprotocol * protocol);
        ~RemoteSet();

        int GetObjectCount() const;
        const NCobject * GetObjectFromIndex(int index) const;
        const NCobject * GetObjectFromUniqueId(int uniqueId) const;
        int GetUniqueIdFromObject(const NCobject * object) const;

	    void ConsumeUpdate(ArithmeticDecoder & decoder, NCpeer * peer);
        void ProduceResponse(ArithmeticEncoder & encoder) const;
    };
}

struct NCobject
{
    virtual const NCclass * GetClass() const = 0;
    virtual int GetInt(const NCint * field) const = 0;
    virtual const NCobject * GetRef(const NCref * field) const = 0;
    virtual void SetVisibility(NCpeer * peer, bool isVisible) const {}

    virtual void SetInt(const NCint * f, int value) {}
    virtual void SetRef(const NCref * f, const NCobject * value) {}
    virtual void Destroy() {}
};

struct NCauthority
{
	const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
	std::vector<netcode::LocalObject *> objects;
    std::vector<netcode::LocalObject *> events;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<netcode::LocalObject *>> eventHistory;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	NCauthority(const NCprotocol * protocol);
    ~NCauthority();

    void PurgeReferencesToObject(NCobject * object);

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    NCpeer * CreatePeer();
	netcode::LocalObject * CreateObject(const NCclass * objectClass);
    void PublishFrame();
};

struct netcode::LocalObject : public NCobject
{
    NCauthority * auth;
    const NCclass * cl;
    std::vector<uint8_t> constState;
	int varStateOffset;
    bool isPublished;

	LocalObject(NCauthority * auth, const NCclass * cl);

    const NCclass * GetClass() const override { return cl; }
    int GetInt(const NCint * field) const override;
    const NCobject * GetRef(const NCref * field) const override;
    void SetVisibility(NCpeer * peer, bool isVisible) const override;

    void SetInt(const NCint * f, int value) override;
    void SetRef(const NCref * f, const NCobject * value) override;
    void Destroy() override;
};

struct NCpeer
{
    NCauthority * auth;
    netcode::LocalSet local;
    netcode::RemoteSet remote;

    NCpeer(NCauthority * auth);
    ~NCpeer();

    int GetNetId(const NCobject * object, int frame) const;

    std::vector<uint8_t> ProduceMessage();
    void ConsumeMessage(const void * data, int size);
};

#endif