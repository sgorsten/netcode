#ifndef POLICY_H
#define POLICY_H

#include "Common.h"
#include "Util.h"

struct VBlob_
{
    std::vector<uint8_t> memory;
};

struct VClass_
{
	int numIntFields;
};

struct Policy
{
    struct Field { int offset, distIndex; };

    struct Class
    {
        VClass_ * cl;
        int index, sizeBytes;
        std::vector<Field> fields;
    };

    std::vector<Class> classes;
    size_t numIntFields;
    int maxFrameDelta;

    Policy(VClass_ * const classes[], size_t numClasses, int maxFrameDelta);
};

struct Distribs
{
    std::vector<IntegerDistribution> intFieldDists;
	IntegerDistribution newObjectCountDist, delObjectCountDist, uniqueIdDist;
    SymbolDistribution classDist;

    Distribs() {}
    Distribs(const Policy & policy) : intFieldDists(policy.numIntFields), classDist(policy.classes.size()) {}
};

#endif