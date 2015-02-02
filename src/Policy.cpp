#include "Policy.h"

Policy::Policy(VClass_ * const classes[], size_t numClasses, int maxFrameDelta) : numIntFields(), maxFrameDelta(maxFrameDelta)
{
    for(size_t i=0; i<numClasses; ++i)
    {
        Class cl;
        cl.cl = classes[i];
        cl.index = i;
        cl.sizeBytes = 0;
        for(int fieldIndex = 0; fieldIndex < classes[i]->numIntFields; ++fieldIndex)
        {
            cl.fields.push_back({cl.sizeBytes, numIntFields++});
            cl.sizeBytes += sizeof(int);
        }
        this->classes.push_back(cl);
    }
}