// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "netcodex.h"
#include "object.h"

namespace netcode
{
    /*template<class A, class B> size_t MemUsage(const std::pair<A,B> & pair) { return MemUsage(pair.first) + MemUsage(pair.second); }
    template<class T> size_t MemUsage(const std::shared_ptr<T> & ptr) { return ptr ? MemUsage(ptr.get()) : 0; }
    template<class T, int N> size_t MemUsage(const T (& arr)[N])
    {
        size_t total = 0;
        for(auto & elem : arr) total += MemUsage(elem);
        return total;
    }
    template<class T> size_t MemUsage(const std::vector<T> & vec)
    {
        size_t total = vec.capacity() * sizeof(T);
        for(auto & elem : vec) total += MemUsage(elem);
        return total;
    }
    template<class K, class V> size_t MemUsage(const std::map<K,V> & map)
    {
        size_t total = map.size() * sizeof(std::pair<K,V>);
        for(auto & elem : map) total += MemUsage(elem);
        return total;
    }

    static size_t MemUsage(uint8_t) { return 0; }
    static size_t MemUsage(int) { return 0; }
    static size_t MemUsage(const NCpeer::ObjectRecord & r) { return 0; }
    static size_t MemUsage(const IntegerDistribution & d) { return 0; }
    static size_t MemUsage(const FieldDistribution & d) { return MemUsage(d.dists); }
    static size_t MemUsage(const Distribs & d) { return MemUsage(d.intFieldDists); }
    static size_t MemUsage(const NCint * f) { return sizeof(NCint); }
    static size_t MemUsage(const NCclass * cl) { return sizeof(NCclass) + MemUsage(cl->constFields) + MemUsage(cl->varFields); }
    static size_t MemUsage(const NCprotocol * p) { return sizeof(NCprotocol) + MemUsage(p->objectClasses) + MemUsage(p->eventClasses); }
    static size_t MemUsage(const NCobject * obj) { return sizeof(NCobject); }
    static size_t MemUsage(const Client::Frame & f) { return MemUsage(f.views) + MemUsage(f.distribs); }
    static size_t MemUsage(const Client & client) { return MemUsage(client.frames); }
    static size_t MemUsage(const NCpeer * peer) { return sizeof(NCpeer) + MemUsage(peer->records) + MemUsage(peer->visChanges) + MemUsage(peer->frameDistribs) + MemUsage(peer->client); }
    static size_t MemUsage(const netcode::RemoteObject * peer) { return sizeof(NCview); }    */

    static void PrintEfficiency(const NCclass & cl, const Distribs & distribs)
    {
        float createCost = 0, updateCost = 0;
        for(size_t j=0; j<cl.constFields.size(); ++j)
        {
            auto & dist = distribs.intConstDists[cl.constFields[j]->uniqueId];
            float cost = dist.GetExpectedCost();
            printf("  constant %d: %f bits\n", j, cost);
            createCost += cost;
        }
        for(size_t j=0; j<cl.varFields.size(); ++j)
        {
            auto & dist = distribs.intFieldDists[cl.varFields[j]->uniqueId];
            int best = dist.GetBestDistribution(4);
            float cost = dist.dists[best].GetExpectedCost();
            printf("  variable %d: %f bits ", j, cost);
            switch(best)
            {
            case 0: printf("(zero predictor)\n"); break;
            case 1: printf("(constant predictor)\n"); break;
            case 2: printf("(linear predictor)\n"); break;
            case 3: printf("(quadratic predictor)\n"); break;
            case 4: printf("(cubic predictor)\n"); break;
            }
            createCost += dist.dists[0].GetExpectedCost();
            updateCost += cost;
        }
        printf("  total: %f bits to create, %f bits to update.\n\n", createCost, updateCost);
    }

    static void PrintEfficiency(const NCprotocol & protocol, const Distribs & distribs)
    {
        //auto client = &peer->client;
        //auto it = client->frames.rbegin();
        //if(it == client->frames.rend()) return;
        //const auto & distribs = it->second.distribs;

        float idCost = distribs.uniqueIdDist.GetExpectedCost(), classCost = distribs.objectClassDist.GetExpectedCost(), newUnitCost = idCost + classCost;
        double avgNewUnits = distribs.newObjectCountDist.GetAverageValue();

        float headerCost = 0;
        headerCost += sizeof(int32_t)*8.0f;
        headerCost += (float)log(protocol.maxFrameDelta+1) * 4;
        headerCost += distribs.newObjectCountDist.GetExpectedCost();
        headerCost += newUnitCost * (float)avgNewUnits;
        headerCost += distribs.delObjectCountDist.GetExpectedCost();

        printf("update header: %f bits\n", headerCost);
        printf("  frame counter:        %f bits\n", sizeof(int32_t)*8.0f);
        printf("  prev frame deltas:    %f bits\n", log(protocol.maxFrameDelta+1) * 4);
        printf("  new object count:     %f bits\n", distribs.newObjectCountDist.GetExpectedCost());
        printf("  new objects:          %f bits\n", newUnitCost * avgNewUnits);
        printf("    # per frame:        %f objects\n", avgNewUnits);
        printf("    unique id:          %f bits per object\n", idCost);
        printf("    class index:        %f bits per object\n", classCost);
        printf("  deleted object count: %f bits\n", distribs.delObjectCountDist.GetExpectedCost());
        printf("  deleted units:        ??? bits\n");
        printf("    # per frame:        %f objects\n", distribs.delObjectCountDist.GetAverageValue());
        printf("    object index:       ??? bits\n\n");

        for(size_t i=0; i<protocol.objectClasses.size(); ++i)
        {
            if(distribs.objectClassDist.GetTrueProbability(i) == 0) continue;
            printf("object class %d: (%f%%)\n", i, distribs.objectClassDist.GetTrueProbability(i) * 100);
            PrintEfficiency(*protocol.objectClasses[i], distribs);
        }

        for(size_t i=0; i<protocol.eventClasses.size(); ++i)
        {
            if(distribs.eventClassDist.GetTrueProbability(i) == 0) continue;
            printf("event class %d: (%f%%)\n", i, distribs.eventClassDist.GetTrueProbability(i) * 100);
            PrintEfficiency(*protocol.eventClasses[i], distribs);
        }
    }
}

using namespace netcode;

int ncxGetMemoryUsage(NCauthority * authority)
{
    return 0; //sizeof(NCauthority) + MemUsage(authority->protocol) + MemUsage(authority->objects) + MemUsage(authority->peers) + MemUsage(authority->state) + MemUsage(authority->frameState);
}

void ncxPrintCodeEfficiency (NCpeer * peer)
{
    /*auto client = &peer->client;
    auto it = client->frames.rbegin();
    if(it == client->frames.rend()) return;
    const auto & distribs = it->second.distribs;

    if(!peer->client.frames.empty())
    {
        printf("\nincoming code efficiency:\n\n");
        PrintEfficiency(*peer->auth->protocol, peer->client.frames.rbegin()->second.distribs);
    }

    if(!peer->frameDistribs.empty())
    {
        printf("\noutgoing code efficiency:\n\n");
        PrintEfficiency(*peer->auth->protocol, peer->frameDistribs.rbegin()->second);
    }*/
}

#include <iostream>

void ncxPrintFrameset (NCpeer * peer, const char * label)
{
    /*std::cout << label << " - ";
    for(auto f : peer->ackFrames) std::cout << f << " ";
    if(!peer->client.frames.empty()) std::cout << "- " << peer->client.frames.rbegin()->first;
    std::cout << std::endl;*/
}