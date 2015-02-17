// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "protocol.h"
#include <cassert>

using namespace netcode;

NCint::NCint(NCclass * cl, int flags) : cl(cl), isConst(flags & NC_CONST_FIELD_FLAG)
{ 
    if(isConst)
    {
        uniqueId = cl->protocol->numIntConstants++;
        dataOffset = cl->constSizeInBytes;
        cl->constSizeInBytes += sizeof(int32_t); 
        cl->constFields.push_back(this);
    }
    else
    {
        uniqueId = cl->protocol->numIntFields++;
        dataOffset = cl->varSizeInBytes;
        cl->varSizeInBytes += sizeof(int32_t); 
        cl->varFields.push_back(this);
    }
}

NCclass::NCclass(NCprotocol * protocol, bool isEvent) : protocol(protocol), isEvent(isEvent), uniqueId(isEvent ? protocol->eventClasses.size() : protocol->objectClasses.size()), constSizeInBytes(0), varSizeInBytes(0)
{
    if(isEvent) protocol->eventClasses.push_back(this);
    else protocol->objectClasses.push_back(this);
}

NCprotocol::NCprotocol(int maxFrameDelta) : maxFrameDelta(maxFrameDelta), numIntFields(0), numIntConstants(0)
{
    
}

//////////////
// Distribs //
//////////////

Distribs::Distribs()
{

}

Distribs::Distribs(const NCprotocol & protocol) : 
    intFieldDists(protocol.numIntFields), intConstDists(protocol.numIntConstants), objectClassDist(protocol.objectClasses.size()), eventClassDist(protocol.eventClasses.size()) 
{

}

void Distribs::EncodeAndTallyObjectConstants(ArithmeticEncoder & encoder, const NCclass & cl, const std::vector<uint8_t> & state)
{
    for(auto field : cl.constFields)
	{
        intConstDists[field->uniqueId].EncodeAndTally(encoder, reinterpret_cast<const int &>(state[field->dataOffset]));
	}    
}

std::vector<uint8_t> Distribs::DecodeAndTallyObjectConstants(ArithmeticDecoder & decoder, const NCclass & cl)
{
    std::vector<uint8_t> state(cl.constSizeInBytes);
    for(auto field : cl.constFields)
	{
        reinterpret_cast<int &>(state[field->dataOffset]) = intConstDists[field->uniqueId].DecodeAndTally(decoder);
    }
    return state;
}

Frameset::Frameset(const std::vector<int> & frames, const std::map<int, std::vector<uint8_t>> & frameStates) : frame(frames[0])
{
    for(size_t i=0; i<4; ++i)
    {
        prevFrames[i] = i+1 < frames.size() ? frames[i+1] : 0;
        auto it = frameStates.find(prevFrames[i]);
        prevStates[i] = it != end(frameStates) ? it->second.data() : nullptr;
    }

    predictors[0] = CurvePredictor();
    predictors[1] = prevFrames[0] != 0 ? MakeConstantPredictor() : predictors[0];
    predictors[2] = prevFrames[1] != 0 ? MakeLinearPredictor(frame-prevFrames[0], frame-prevFrames[1]) : predictors[1];
    predictors[3] = prevFrames[2] != 0 ? MakeQuadraticPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2]) : predictors[1];
    predictors[4] = prevFrames[3] != 0 ? MakeCubicPredictor(frame-prevFrames[0], frame-prevFrames[1], frame-prevFrames[2], frame-prevFrames[3]) : predictors[1];
}

int Frameset::GetSampleCount(int frameAdded) const 
{ 
    for(int i=4; i>0; --i)
    {
        if(frameAdded <= prevFrames[i-1]) return i; 
    }
    return 0; 
}

void Frameset::EncodeAndTallyObject(ArithmeticEncoder & encoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, const uint8_t * state) const
{
    const int sampleCount = GetSampleCount(frameAdded);
    for(auto field : cl.varFields)
	{
        int offset = stateOffset + field->dataOffset, prevValues[4];
        for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
		distribs.intFieldDists[field->uniqueId].EncodeAndTally(encoder, reinterpret_cast<const int &>(state[offset]), prevValues, predictors, sampleCount);
	}    
}

void Frameset::DecodeAndTallyObject(ArithmeticDecoder & decoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, uint8_t * state) const
{
    const int sampleCount = GetSampleCount(frameAdded);
    for(auto field : cl.varFields)
	{
        int offset = stateOffset + field->dataOffset, prevValues[4];
        for(int i=0; i<4; ++i) prevValues[i] = sampleCount > i ? reinterpret_cast<const int &>(prevStates[i][offset]) : 0;
		reinterpret_cast<int &>(state[offset]) = distribs.intFieldDists[field->uniqueId].DecodeAndTally(decoder, prevValues, predictors, sampleCount);
	}    
}

void netcode::EncodeFramelist(ArithmeticEncoder & encoder, const int * frames, size_t numFrames, size_t maxFrames, int maxFrameDelta)
{
    assert(numFrames <= maxFrames);
    EncodeUniform(encoder, numFrames, maxFrames+1);
    if(numFrames) EncodeBits(encoder, frames[0], 32);
    for(size_t i=1; i<numFrames; ++i)
    {
        auto delta = frames[i-1] - frames[i];
        EncodeUniform(encoder, delta, maxFrameDelta+1);
        maxFrameDelta -= delta;
    }
}

std::vector<int> netcode::DecodeFramelist(ArithmeticDecoder & decoder, size_t maxFrames, int maxFrameDelta)
{
    std::vector<int> frames;
    size_t numFrames = DecodeUniform(decoder, maxFrames+1);    
    if(numFrames) frames.push_back(DecodeBits(decoder, 32));
    for(size_t i=1; i<numFrames; ++i)
    {
        auto delta = DecodeUniform(decoder, maxFrameDelta+1);
        frames.push_back(frames.back() - delta);
        maxFrameDelta -= delta;
    }
    return frames;
}