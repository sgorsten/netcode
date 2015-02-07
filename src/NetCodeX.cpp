#include "netcodex.h"

#include "Server.h"
#include "Client.h"

namespace netcode
{
    template<class A, class B> size_t MemUsage(const std::pair<A,B> & pair) { return MemUsage(pair.first) + MemUsage(pair.second); }
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
    static size_t MemUsage(const NCclass * cl) { return sizeof(NCclass); }
    static size_t MemUsage(const Policy::Field & f) { return 0; }
    static size_t MemUsage(const Policy::Class & cl) { return MemUsage(cl.cl) + MemUsage(cl.fields); }
    static size_t MemUsage(const Policy & policy) { return MemUsage(policy.classes); }
    static size_t MemUsage(const NCobject * obj) { return sizeof(NCobject); }
    static size_t MemUsage(const NCpeer * peer) { return sizeof(NCpeer) + MemUsage(peer->records) + MemUsage(peer->visChanges) + MemUsage(peer->frameDistribs); }
    static size_t MemUsage(const NCview * peer) { return sizeof(NCview); }
    static size_t MemUsage(const ClientFrame & f) { return MemUsage(f.views) + MemUsage(f.state) + MemUsage(f.distribs); }
}

using namespace netcode;

int ncxServerMemoryUsage(NCserver * server)
{
    return sizeof(NCserver) + MemUsage(server->policy) + MemUsage(server->objects) + MemUsage(server->peers) + MemUsage(server->state) + MemUsage(server->frameState);
}

int ncxClientMemoryUsage(NCclient * client)
{
    return sizeof(NCclient) + MemUsage(client->policy) + MemUsage(client->frames);
}

void ncxPrintClientCodeEfficiency (struct NCclient * client)
{
    auto it = client->frames.rbegin();
    if(it == client->frames.rend()) return;
    const auto & distribs = it->second.distribs;

    printf("update header:\n");
    printf("  new object count: %f bits\n", distribs.newObjectCountDist.GetExpectedCost());
    printf("  deleted object count: %f bits\n\n", distribs.delObjectCountDist.GetExpectedCost());

    printf("new object:\n");
    printf("  unique id: %f bits\n", distribs.uniqueIdDist.GetExpectedCost());
    printf("  class index: %f bits\n", distribs.classDist.GetExpectedCost());
    printf("  total: %f bits per object\n\n", distribs.uniqueIdDist.GetExpectedCost() + distribs.classDist.GetExpectedCost());

    const auto & classes = client->policy.classes;
    for(int i=0; i<classes.size(); ++i)
    {
        printf("class %d:\n", i);
        float totalCost = 0;
        for(int j=0; j<classes[i].fields.size(); ++j)
        {
            auto & dist = distribs.intFieldDists[classes[i].fields[j].distIndex];
            int best = dist.GetBestDistribution(4);
            float cost = dist.dists[best].GetExpectedCost();
            printf("  field %d: %f bits", j, cost);
            switch(best)
            {
            case 0: printf("(zero predictor)\n"); break;
            case 1: printf("(constant predictor)\n"); break;
            case 2: printf("(linear predictor)\n"); break;
            case 3: printf("(quadratic predictor)\n"); break;
            case 4: printf("(cubic predictor)\n"); break;
            }
            totalCost += cost;
        }
        printf("  total: %f bits per object\n\n", totalCost);
    }
}