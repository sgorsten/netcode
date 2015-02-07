#include "Protocol.h"

NCint::NCint(NCclass * cl) : cl(cl), uniqueId(cl->protocol->numIntFields++), dataOffset(cl->sizeInBytes)
{ 
    cl->sizeInBytes += sizeof(int32_t); 
    cl->fields.push_back(this);
}

NCclass::NCclass(NCprotocol * protocol) : protocol(protocol), uniqueId(protocol->classes.size()), sizeInBytes(0)
{
    protocol->classes.push_back(this);
}

NCprotocol::NCprotocol(int maxFrameDelta) : maxFrameDelta(maxFrameDelta), numIntFields(0)
{
    
}