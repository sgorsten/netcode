#include "Client.h"

#include <cassert>

using namespace netcode;

NCview::NCview(NCclient * client, NCclass * cl, int stateOffset, int frameAdded) : client(client), cl(cl), stateOffset(stateOffset), frameAdded(frameAdded)
{
    
}

NCview::~NCview()
{
    client->stateAlloc.Free(stateOffset, cl->sizeInBytes);
}

int NCview::GetIntField(NCint * field) const
{ 
    if(field->cl != cl) return 0;
    return reinterpret_cast<const int &>(client->GetCurrentState()[stateOffset + field->dataOffset]); 
}

NCclient::NCclient(NCprotocol * protocol) : protocol(protocol)
{

}

std::shared_ptr<NCview> NCclient::CreateView(size_t classIndex, int uniqueId, int frameAdded)
{
    auto it = id2View.find(uniqueId);
    if(it != end(id2View))
    {
        if(auto ptr = it->second.lock())
        {
            assert(ptr->cl->uniqueId == classIndex);
            return ptr;
        }        
    }

    auto cl = protocol->classes[classIndex];
    auto ptr = std::make_shared<NCview>(this, cl, (int)stateAlloc.Allocate(cl->sizeInBytes), frameAdded);
    id2View[uniqueId] = ptr;
    return ptr;
}

void NCclient::ConsumeUpdate(const uint8_t * buffer, size_t bufferSize)
{
    if(bufferSize < 4) return;
    int32_t frame, prevFrames[4];
    const uint8_t * prevStates[4];

    // Don't bother decoding messages for old frames (TODO: We may still want to decode these frames if they can improve our ack set)
    memcpy(&frame, buffer + 0, sizeof(int32_t));
    if(!frames.empty() && frames.rbegin()->first >= frame) return;

    // Prepare arithmetic code for this frame
	std::vector<uint8_t> bytes(buffer + 4, buffer + bufferSize);
	ArithmeticDecoder decoder(bytes);
    for(int i=0; i<4; ++i)
    {
        prevFrames[i] = DecodeUniform(decoder, protocol->maxFrameDelta+1);
        if(prevFrames[i]) prevFrames[i] = frame - prevFrames[i];
        prevStates[i] = GetFrameState(prevFrames[i]);
        if(prevFrames[i] != 0 && prevStates[i] == nullptr) return; // Malformed packet
    }

    CurvePredictor predictors[5];
    predictors[1] = prevFrames[0] != 0 ? MakeConstantPredictor() : predictors[0];
    predictors[2] = prevFrames[1] != 0 ? MakeLinearPredictor(frame-prevFrames[0], frame-prevFrames[1]) : predictors[1];
    predictors[3] = prevFrames[2] != 0 ? MakeQuadraticPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2]) : predictors[1];
    predictors[4] = prevFrames[3] != 0 ? MakeCubicPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2], frame-prevFrames[3]) : predictors[1];

    auto & distribs = frames[frame].distribs;
    auto & views = frames[frame].views;
    if(prevFrames[0] != 0)
    {
        distribs = frames[prevFrames[0]].distribs;
        views = frames[prevFrames[0]].views;
    }
    else distribs = Distribs(*protocol);

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
        int sampleCount = 0;
        for(int i=4; i>0; --i)
        {
            if(view->IsLive(prevFrames[i-1]))
            {
                sampleCount = i;
                break;
            }
        }

		for (auto field : view->cl->fields)
		{
            int offset = view->stateOffset + field->dataOffset;
            int prevValues[4];
            for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
            reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field->uniqueId].DecodeAndTally(decoder, prevValues, predictors, sampleCount);
		}
	}

    // Server will never again refer to frames before this point
    frames.erase(begin(frames), frames.lower_bound(std::min(frame - protocol->maxFrameDelta, prevFrames[3])));
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
        if(buffer.size() == 16) break;
    }
    return buffer;
}