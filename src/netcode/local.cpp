#include "object.h"
#include <cassert>

using namespace netcode;

struct LocalSet::Record
{
    const netcode::LocalObject * object; 
    int uniqueId, frameAdded, frameRemoved; 

    bool IsLive(int frame) const { return frameAdded <= frame && frame < frameRemoved; }
};

LocalSet::LocalSet(const NCauthority * auth) : auth(auth), nextId(1)
{

}

LocalSet::~LocalSet()
{

}

const NCobject * LocalSet::GetObjectFromUniqueId(int uniqueId) const
{
    for(auto & record : records)
    {
        if(record.uniqueId == uniqueId) return record.object;
    }
    return nullptr;
}

int LocalSet::GetUniqueIdFromObject(const NCobject * object, int frame) const
{
    for(auto & record : records)
    {
        if(record.object == object && record.IsLive(frame))
        {
            return record.uniqueId;
        }
    }
    return 0;
}

void LocalSet::OnPublishFrame(int frame)
{
    if(!auth) return;

    for(auto change : visChanges)
    {
        auto it = std::find_if(begin(records), end(records), [=](Record & r) { return r.object == change.first && r.IsLive(frame); });
        if((it != end(records)) == change.second) continue; // If object visibility is as desired, skip this change
        if(change.second) records.push_back({change.first, nextId++, frame, INT_MAX}); // Make object visible
        else it->frameRemoved = frame; // Make object invisible
    }
    visChanges.clear();

    int oldestAck = GetOldestAckFrame();
    EraseIf(records, [=](Record & r) { return r.frameRemoved < oldestAck || r.frameRemoved < auth->frame - auth->protocol->maxFrameDelta; });
    frameDistribs.erase(begin(frameDistribs), frameDistribs.lower_bound(std::min(auth->frame - auth->protocol->maxFrameDelta, oldestAck)));
}

void LocalSet::SetVisibility(const LocalObject * object, bool setVisible)
{
    if(!auth) return;

    if(object->cl->isEvent)
    {
        if(object->isPublished) return;
        if(setVisible) visibleEvents.insert(object);
        else visibleEvents.erase(object);
    }
    else
    {
        visChanges.push_back({object,setVisible});
    }
}

void LocalSet::ProduceUpdate(ArithmeticEncoder & encoder, NCpeer * peer)
{
    std::vector<int> frameList = {auth->frame};
    int32_t cutoff = auth->frame - auth->protocol->maxFrameDelta;
    for(auto frame : ackFrames) if(frame >= cutoff) frameList.push_back(frame); // TODO: Enforce this in PublishFrame instead

    netcode::EncodeFramelist(encoder, frameList.data(), frameList.size(), 5, auth->protocol->maxFrameDelta);
    const Frameset frameset(frameList, auth->frameState);

    // Obtain probability distributions for this frame
    auto & distribs = frameDistribs[frameset.GetCurrentFrame()];
    if(frameset.GetPreviousFrame() != 0) distribs = frameDistribs[frameset.GetPreviousFrame()];
    else distribs = Distribs(*auth->protocol);

    // Encode visible events that occurred in each frame between the last acknowledged frame and the current frame
    std::vector<const LocalObject *> sendEvents;
    for(int i=frameset.GetPreviousFrame()+1; i<=frameset.GetCurrentFrame(); ++i)
    {
        sendEvents.clear();
        for(auto e : auth->eventHistory.find(i)->second) if(visibleEvents.find(e) != end(visibleEvents)) sendEvents.push_back(e);
        distribs.eventCountDist.EncodeAndTally(encoder, sendEvents.size());
        for(auto e : sendEvents)
        {
            distribs.eventClassDist.EncodeAndTally(encoder, e->cl->uniqueId);
            distribs.EncodeAndTallyObjectConstants(encoder, *e->cl, e->constState);
        }
    }

    // Encode the indices of destroyed objects
    std::vector<int> deletedIndices;
    std::vector<const Record *> newObjects;
    int index = 0;
    for(const auto & record : records)
    {
        if(record.IsLive(frameset.GetPreviousFrame()))
        {
            if(!record.IsLive(frameset.GetCurrentFrame())) deletedIndices.push_back(index);  // If it has been removed, store its index
            ++index;                                                    // Either way, object was live, so it used an index
        }
        else if(record.IsLive(frameset.GetCurrentFrame())) // If object was added between last frame and now
        {
            newObjects.push_back(&record);
        }
    }
    int numPrevObjects = index;
    distribs.delObjectCountDist.EncodeAndTally(encoder, deletedIndices.size());
    for(auto index : deletedIndices) EncodeUniform(encoder, index, numPrevObjects);

	// Encode classes of newly created objects
	distribs.newObjectCountDist.EncodeAndTally(encoder, newObjects.size());
	for (auto record : newObjects)
    {
        distribs.objectClassDist.EncodeAndTally(encoder, record->object->cl->uniqueId);
        distribs.uniqueIdDist.EncodeAndTally(encoder, record->uniqueId);
        distribs.EncodeAndTallyObjectConstants(encoder, *record->object->cl, record->object->constState);
    }

	// Encode updates for each view
    auto state = auth->GetFrameState(frameset.GetCurrentFrame());
    for(const auto & record : records)
    {
        if(record.IsLive(frameset.GetCurrentFrame()))
        {
            frameset.EncodeAndTallyObject(encoder, distribs, *record.object->cl, record.object->varStateOffset, record.frameAdded, state);

            for(auto field : record.object->cl->varRefs)
            {
                auto offset = record.object->varStateOffset + field->dataOffset;
                auto value = reinterpret_cast<const NCobject * const &>(state[offset]);
                auto prevValue = record.IsLive(frameset.GetPreviousFrame()) ? reinterpret_cast<const NCobject * const &>(auth->GetFrameState(frameset.GetPreviousFrame())[offset]) : nullptr;
                auto id = peer->GetNetId(value, frameset.GetCurrentFrame());
                auto prevId = peer->GetNetId(prevValue, frameset.GetPreviousFrame());
                distribs.uniqueIdDist.EncodeAndTally(encoder, id-prevId);
            }
        }
    }
}

void LocalSet::ConsumeResponse(ArithmeticDecoder & decoder) 
{
    if(!auth) return;
    auto newAck = netcode::DecodeFramelist(decoder, 4, auth->protocol->maxFrameDelta);
    if(newAck.empty()) return;
    if(ackFrames.empty() || ackFrames.front() < newAck.front()) ackFrames = newAck;
}

void LocalSet::PurgeReferences()
{
    auth = nullptr;
    records.clear();
    visibleEvents.clear();
    visChanges.clear();      
}
