#include "Client.h"

#include <cassert>

NCview::NCview(NCclient * client, const Policy::Class & cl, int stateOffset, int frameAdded) : client(client), cl(cl), stateOffset(stateOffset), frameAdded(frameAdded)
{
    
}

NCview::~NCview()
{
    client->stateAlloc.Free(stateOffset, cl.sizeBytes);
}

int NCview::GetIntField(int index) const
{ 
    return reinterpret_cast<const int &>(client->GetCurrentState()[stateOffset + cl.fields[index].offset]); 
}

NCclient::NCclient(NCclass * const classes[], size_t numClasses, int maxFrameDelta) : policy(classes, numClasses, maxFrameDelta)
{

}

std::shared_ptr<NCview> NCclient::CreateView(size_t classIndex, int uniqueId, int frameAdded)
{
    auto it = id2View.find(uniqueId);
    if(it != end(id2View))
    {
        if(auto ptr = it->second.lock())
        {
            assert(ptr->cl.index == classIndex);
            return ptr;
        }        
    }

    auto & cl = policy.classes[classIndex];
    auto ptr = std::make_shared<NCview>(this, cl, (int)stateAlloc.Allocate(cl.sizeBytes), frameAdded);
    id2View[uniqueId] = ptr;
    return ptr;
}

void NCclient::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
    if(bufferSize < 4) return;
    int32_t frame, prevFrame, prevPrevFrame;
    memcpy(&frame, buffer + 0, sizeof(int32_t));

    // Don't bother decoding messages for old frames (TODO: We may still want to decode these frames if they can improve our ack set)
    if(!frames.empty() && frames.rbegin()->first >= frame) return;

    // Prepare arithmetic code for this frame
	std::vector<uint8_t> bytes(buffer + 4, buffer + bufferSize);
	arith::Decoder decoder(bytes);
    prevFrame = DecodeUniform(decoder, policy.maxFrameDelta+1);
    prevPrevFrame = DecodeUniform(decoder, policy.maxFrameDelta+1);
    if(prevFrame) prevFrame = frame - prevFrame;
    if(prevPrevFrame) prevPrevFrame = frame - prevPrevFrame;
    
    auto prevState = GetFrameState(prevFrame);
    auto prevPrevState = GetFrameState(prevPrevFrame);
    if(prevFrame != 0 && prevState == nullptr) return; // Malformed
    if(prevPrevFrame != 0 && prevPrevState == nullptr) return; // Malformed

    auto & distribs = frames[frame].distribs;
    auto & views = frames[frame].views;
    if(prevFrame != 0)
    {
        distribs = frames[prevFrame].distribs;
        views = frames[prevFrame].views;
    }
    else distribs = Distribs(policy);

    // Decode indices of deleted objects
    int delObjects = distribs.delObjectCountDist.DecodeAndTally(decoder);
    for(int i=0; i<delObjects; ++i)
    {
        int index = DecodeUniform(decoder, views.size());
        views[index].reset();
    }
    EraseIf(views, [](const std::shared_ptr<NCview> & v) { return !v; });

	// Decode classes of newly created objects, and instantiate corresponding views
	int newObjects = distribs.newObjectCountDist.DecodeAndTally(decoder);
	for (int i = 0; i < newObjects; ++i)
	{
        auto classIndex = distribs.classDist.DecodeAndTally(decoder);
        auto uniqueId = distribs.uniqueIdDist.DecodeAndTally(decoder);
        views.push_back(CreateView(classIndex, uniqueId, frame));
	}

    auto & state = frames[frame].state;
    state.resize(stateAlloc.GetTotalCapacity());

	// Decode updates for each view
	for (auto view : views)
	{
		for (auto & field : view->cl.fields)
		{
            int offset = view->stateOffset + field.offset;
            int prevValue = view->IsLive(prevFrame) ? reinterpret_cast<const int &>(prevState[offset]) : 0;
            int prevPrevValue = view->IsLive(prevPrevFrame) ? reinterpret_cast<const int &>(prevPrevState[offset]) : 0;
            reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field.distIndex].DecodeAndTally(decoder) + (prevValue * 2 - prevPrevValue);
		}
	}

    // Server will never again refer to frames before this point
    frames.erase(begin(frames), frames.lower_bound(std::min(frame - policy.maxFrameDelta, prevPrevFrame)));
    for(auto it = id2View.begin(); it != end(id2View); )
    {
        if(it->second.expired()) it = id2View.erase(it);
        else ++it;
    }
}

std::vector<uint8_t> NCclient::ProduceResponse()
{
    std::vector<uint8_t> buffer;
    for(auto it = frames.rbegin(); it != frames.rend(); ++it)
    {
        auto offset = buffer.size();
        buffer.resize(offset + 4);
        memcpy(buffer.data() + offset, &it->first, sizeof(int32_t));
        if(buffer.size() == 8) break;
    }
    return buffer;
}