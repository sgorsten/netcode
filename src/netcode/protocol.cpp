// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "protocol.h"

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