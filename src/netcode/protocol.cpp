// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "protocol.h"

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