#ifndef NETIOMP_H
#define NETIOMP_H
#include <optional>
#include <unistd.h>
#include <emp-tool/emp-tool.h>
#include <emp-tool/io/net_io.h>
#include "cmpc_config.h"
#include "vec.h"
using namespace emp;

class NetIOMP { public:
    int nP;
    Vec<std::optional<IOChannel>> ios;
    Vec<std::optional<IOChannel>> ios2;
    int party;

    NetIOMP(int nP, int party, int port)
    :
        nP(nP),
        ios(nP+1),
        ios2(nP+1),
        party(party)
    {
        for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
            if(i == party) {
#ifdef LOCALHOST
                usleep(1000);
                ios[j].emplace(make_net_io(IP[j], port+2*(i*nP+j)));
#else
                usleep(1000);
                ios[j].emplace(make_net_io(IP[j], port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                ios2[j].emplace(make_net_io(nullptr, port+2*(i*nP+j)+1));
#else
                usleep(1000);
                ios2[j].emplace(make_net_io(nullptr, port+2*(j)+1));
#endif
            } else if(j == party) {
#ifdef LOCALHOST
                usleep(1000);
                ios[i].emplace(make_net_io(nullptr, port+2*(i*nP+j)));
#else
                usleep(1000);
                ios[i].emplace(make_net_io(nullptr, port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                ios2[i].emplace(make_net_io(IP[i], port+2*(i*nP+j)+1));
#else
                usleep(1000);
                ios2[i].emplace(make_net_io(IP[i], port+2*(j)+1));
#endif
            }
        }
    }
    int64_t count() {
        int64_t res = 0;
        for(int i = 1; i <= nP; ++i) if(i != party){
            res += *ios[i]->counter;
            res += *ios2[i]->counter;
        }
        return res;
    }

    IOChannel& send_channel(int party2) {
        assert(party2 != 0);
        assert(party2 != party);

        return party < party2 ? *ios[party2] : *ios2[party2];
    }
    IOChannel& recv_channel(int party2) {
        assert(party2 != 0);
        assert(party2 != party);

        return party2 < party ? *ios[party2] : *ios2[party2];
    }

    void flush(int idx) {
        assert(idx != 0);

        if(party < idx)
            ios[idx]->flush();
        else
            ios2[idx]->flush();
    }

    std::shared_ptr<NetIO> make_net_io(const char * address, int port) {
        auto io = std::make_shared<NetIO>(address, port);
        io->flush_all_sends = true;

        return io;
    }
};

#endif // NETIOMP_H
