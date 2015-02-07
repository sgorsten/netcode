#ifndef NETCODE_PROTOCOL_H
#define NETCODE_PROTOCOL_H

#include "Utility.h"

struct NCblob { std::vector<uint8_t> memory; };

struct NCprotocol;
struct NCclass;

struct NCint
{
    NCclass *                cl;             // Class that this field belongs to
    size_t                   uniqueId;       // Unique identifier for this integer field within the protocol
    size_t                   dataOffset;     // Offset into object data where this field's value is stored
    
    NCint(NCclass * cl);
};

struct NCclass
{
    NCprotocol *             protocol;       // Protocol that this class belongs to
    size_t                   uniqueId;       // Unique identifier for this class within the protocol
    size_t                   sizeInBytes;    // Size of this class in bytes
    std::vector<NCint *>     fields;         // Fields of this class

    NCclass(NCprotocol * protocol);
};

struct NCprotocol
{
    int                      maxFrameDelta;  // Maximum difference in frame numbers for frames used in delta compression
    size_t                   numIntFields;   // Number of FieldDistributions used in this protocol
    std::vector<NCclass *>   classes;        // Classes of this protocol

    NCprotocol(int maxFrameDelta);
};

namespace netcode
{
    struct Distribs
    {
        std::vector<FieldDistribution> intFieldDists;
	    IntegerDistribution newObjectCountDist, delObjectCountDist, uniqueIdDist;
        SymbolDistribution classDist;

        Distribs() {}
        Distribs(const NCprotocol & protocol) : intFieldDists(protocol.numIntFields), classDist(protocol.classes.size()) {}
    };
}

#endif