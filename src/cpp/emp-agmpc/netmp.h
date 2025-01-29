#ifndef NETIOMP_H
#define NETIOMP_H
#include <optional>
#include <unistd.h>
#include <emp-tool/emp-tool.h>
#include <emp-tool/io/net_io.h>
#include "cmpc_config.h"
#include "vec.h"
using namespace emp;

class NetIOMP: public IMultiIO {
private:
    int nP;
    Vec<std::optional<IOChannel>> a_channels;
    Vec<std::optional<IOChannel>> b_channels;
    int mParty;

    std::shared_ptr<NetIO> make_net_io(const char * address, int port) {
        return std::make_shared<NetIO>(address, port);
    }

public:
    NetIOMP(int nP, int party, int port)
    :
        nP(nP),
        a_channels(nP+1),
        b_channels(nP+1),
        mParty(party)
    {
        for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
            if(i == party) {
#ifdef LOCALHOST
                usleep(1000);
                a_channels[j].emplace(make_net_io(IP[j], port+2*(i*nP+j)));
#else
                usleep(1000);
                a_channels[j].emplace(make_net_io(IP[j], port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                b_channels[j].emplace(make_net_io(nullptr, port+2*(i*nP+j)+1));
#else
                usleep(1000);
                b_channels[j].emplace(make_net_io(nullptr, port+2*(j)+1));
#endif
            } else if(j == party) {
#ifdef LOCALHOST
                usleep(1000);
                a_channels[i].emplace(make_net_io(nullptr, port+2*(i*nP+j)));
#else
                usleep(1000);
                a_channels[i].emplace(make_net_io(nullptr, port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                b_channels[i].emplace(make_net_io(IP[i], port+2*(i*nP+j)+1));
#else
                usleep(1000);
                b_channels[i].emplace(make_net_io(IP[i], port+2*(j)+1));
#endif
            }
        }
    }

    int party() override {
        return mParty;
    }

    int size() override {
        return nP;
    }

    IOChannel& a_channel(int party2) override {
        assert(party2 != 0);
        assert(party2 != party());

        return *a_channels[party2];
    }
    IOChannel& b_channel(int party2) override {
        assert(party2 != 0);
        assert(party2 != party());

        return *b_channels[party2];
    }

    void flush(int idx) override {
        assert(idx != 0);

        if(party() < idx)
            a_channels[idx]->flush();
        else
            b_channels[idx]->flush();
    }
};

#endif // NETIOMP_H
