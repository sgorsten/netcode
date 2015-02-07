#ifndef NETCODE_POLICY_H
#define NETCODE_POLICY_H

#include "Util.h"

struct NCblob
{
    std::vector<uint8_t> memory;
};

struct NCclass
{
	int numIntFields;
};

namespace netcode
{
    struct Policy
    {
        struct Field { int offset, distIndex; };

        struct Class
        {
            NCclass * cl;
            int index, sizeBytes;
            std::vector<Field> fields;
        };

        std::vector<Class> classes;
        size_t numIntFields;
        int maxFrameDelta;

        Policy(NCclass * const classes[], size_t numClasses, int maxFrameDelta);
    };

    struct Distribs
    {
        std::vector<FieldDistribution> intFieldDists;
	    IntegerDistribution newObjectCountDist, delObjectCountDist, uniqueIdDist;
        SymbolDistribution classDist;

        Distribs() {}
        Distribs(const Policy & policy) : intFieldDists(policy.numIntFields), classDist(policy.classes.size()) {}
    };
}

#endif