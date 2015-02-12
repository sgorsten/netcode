// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#ifndef NETCODE_SERVER_H
#define NETCODE_SERVER_H

#include "protocol.h"

#include <map>

struct NCserver
{
	const NCprotocol * protocol;
    netcode::RangeAllocator stateAlloc;
	std::vector<NCobject *> objects;
    std::vector<NCpeer *> peers;

	std::vector<uint8_t> state;
    std::map<int, std::vector<uint8_t>> frameState;
    int frame;

	NCserver(const NCprotocol * protocol);
    ~NCserver();

    const uint8_t * GetFrameState(int frame) const
    {
        auto it = frameState.find(frame);
        return it != end(frameState) ? it->second.data() : nullptr;
    }

    NCpeer * CreatePeer();
	NCobject * CreateObject(const NCclass * objectClass);
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
    ~NCpeer();

    int GetOldestAckFrame() const { return ackFrames.empty() ? 0 : ackFrames.back(); }
    void OnPublishFrame(int frame);
    void SetVisibility(const NCobject * object, bool setVisible);
    std::vector<uint8_t> ProduceUpdate();
    void ConsumeResponse(const uint8_t * data, size_t size);
};

struct NCobject
{
    NCserver * server;
    const NCclass * cl;
	int stateOffset;

	NCobject(NCserver * server, const NCclass * cl, int stateOffset);
    ~NCobject();

    void SetIntField(const NCint * field, int value);
};

#endif