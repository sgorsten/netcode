// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#ifndef NETCODE_PROTOCOL_H
#define NETCODE_PROTOCOL_H

#include "netcode.h"

#include "utility.h"

struct NCint
{
    NCclass *                cl;             // Class that this field belongs to
    bool                     isConst;        // Whether or not this is a constant field
    size_t                   uniqueId;       // Unique identifier for this integer field within the protocol
    size_t                   dataOffset;     // Offset into object data where this field's value is stored
    
    NCint(NCclass * cl, int flags);
};

struct NCref
{
    NCclass *                cl;             // Class that this field belongs to
    size_t                   dataOffset;     // Offset into object data where this field's value is stored
    
    NCref(NCclass * cl);
};

struct NCclass
{
    NCprotocol *             protocol;          // Protocol that this class belongs to
    bool                     isEvent;           // Whether or not this is an event class
    size_t                   uniqueId;          // Unique identifier for this class within the protocol
    size_t                   constSizeInBytes;  // Size of all constant fields, in bytes
    size_t                   varSizeInBytes;    // Size of all variable fields, in bytes
    std::vector<NCint *>     constFields;       // Constant fields of this class
    std::vector<NCint *>     varFields;         // Variable fields of this class
    std::vector<NCref *>     varRefs;           // Variable fields holding a reference to another object

    NCclass(NCprotocol * protocol, bool isEvent);
};

struct NCprotocol
{
    int                      maxFrameDelta;  // Maximum difference in frame numbers for frames used in delta compression
    size_t                   numIntFields;   // Number of FieldDistributions used in this protocol
    size_t                   numIntConstants;// Number of constant integer fields used in this protocol
    std::vector<NCclass *>   objectClasses;  // Classes used for persistent objects
    std::vector<NCclass *>   eventClasses;   // Classes used for instantaneous events

    NCprotocol(int maxFrameDelta);
};

namespace netcode
{
    struct Distribs
    {
        std::vector<FieldDistribution> intFieldDists;
        std::vector<IntegerDistribution> intConstDists;
	    IntegerDistribution eventCountDist, newObjectCountDist, delObjectCountDist;
        IntegerDistribution uniqueIdDist;
        SymbolDistribution objectClassDist, eventClassDist;

        Distribs();
        Distribs(const NCprotocol & protocol);

        void EncodeAndTallyObjectConstants(ArithmeticEncoder & encoder, const NCclass & cl, const std::vector<uint8_t> & state);
        std::vector<uint8_t> DecodeAndTallyObjectConstants(ArithmeticDecoder & decoder, const NCclass & cl);
    };

    class Frameset
    {
        int32_t frame, prevFrames[4];
        const uint8_t * prevStates[4];
        CurvePredictor predictors[5];

        int GetSampleCount(int frameAdded) const;
    public:
        Frameset(const std::vector<int> & frames, const std::map<int, std::vector<uint8_t>> & frameStates);

        int GetCurrentFrame() const { return frame; }
        int GetPreviousFrame() const { return prevFrames[0]; }
        int GetEarliestFrame() const { return prevFrames[3]; }

        void EncodeAndTallyObject(ArithmeticEncoder & encoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, const uint8_t * state) const;
        void DecodeAndTallyObject(ArithmeticDecoder & decoder, netcode::Distribs & distribs, const NCclass & cl, int stateOffset, int frameAdded, uint8_t * state) const;
    };

    void EncodeFramelist(ArithmeticEncoder & encoder, const int * frames, size_t numFrames, size_t maxFrames, int maxFrameDelta);
    std::vector<int> DecodeFramelist(ArithmeticDecoder & decoder, size_t maxFrames, int maxFrameDelta);
}

#endif