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

int ncDebugServerMemoryUsage(NCserver * server)
{
    return sizeof(NCserver) + MemUsage(server->policy) + MemUsage(server->objects) + MemUsage(server->peers) + MemUsage(server->state) + MemUsage(server->frameState);
}

int ncDebugClientMemoryUsage(NCclient * client)
{
    return sizeof(NCclient) + MemUsage(client->policy) + MemUsage(client->frames);
}