// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "protocol.h"

NCint::NCint(NCclass * cl) : cl(cl), uniqueId(cl->protocol->numIntFields++), dataOffset(cl->sizeInBytes)
{ 
    cl->sizeInBytes += sizeof(int32_t); 
    cl->fields.push_back(this);
}

NCclass::NCclass(NCprotocol * protocol, bool isEvent) : protocol(protocol), isEvent(isEvent), uniqueId(protocol->classes.size()), sizeInBytes(0)
{
    // TODO: Possibly store object and event classes in separate lists, with separate IDs
    protocol->classes.push_back(this);
}

NCprotocol::NCprotocol(int maxFrameDelta) : maxFrameDelta(maxFrameDelta), numIntFields(0)
{
    
}