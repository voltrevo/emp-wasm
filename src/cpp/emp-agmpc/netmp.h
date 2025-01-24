#ifndef NETIOMP_H
#define NETIOMP_H
#include <unistd.h>
#include <emp-tool/emp-tool.h>
#include <emp-tool/io/net_io.h>
#include "cmpc_config.h"
using namespace emp;

template<int nP>
class NetIOMP { public:
    IOChannel*ios[nP+1];
    IOChannel*ios2[nP+1];
    int party;
    bool sent[nP+1];
    NetIOMP(int party, int port) {
        this->party = party;
        memset(sent, false, nP+1);
        for(int i = 1; i <= nP; ++i)for(int j = 1; j <= nP; ++j)if(i < j){
            if(i == party) {
#ifdef LOCALHOST
                usleep(1000);
                ios[j] = new IOChannel(std::make_shared<NetIO>(IP[j], port+2*(i*nP+j)));
#else
                usleep(1000);
                ios[j] = new IOChannel(std::make_shared<NetIO>(IP[j], port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                ios2[j] = new IOChannel(std::make_shared<NetIO>(nullptr, port+2*(i*nP+j)+1));
#else
                usleep(1000);
                ios2[j] = new IOChannel(std::make_shared<NetIO>(nullptr, port+2*(j)+1));
#endif
            } else if(j == party) {
#ifdef LOCALHOST
                usleep(1000);
                ios[i] = new IOChannel(std::make_shared<NetIO>(nullptr, port+2*(i*nP+j)));
#else
                usleep(1000);
                ios[i] = new IOChannel(std::make_shared<NetIO>(nullptr, port+2*(i)));
#endif

#ifdef LOCALHOST
                usleep(1000);
                ios2[i] = new IOChannel(std::make_shared<NetIO>(IP[i], port+2*(i*nP+j)+1));
#else
                usleep(1000);
                ios2[i] = new IOChannel(std::make_shared<NetIO>(IP[i], port+2*(j)+1));
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

    ~NetIOMP() {
        for(int i = 1; i <= nP; ++i)
            if(i != party) {
                delete ios[i];
                delete ios2[i];
            }
    }
    void send_data(int dst, const void * data, size_t len) {
        if(dst != 0 and dst!= party) {
            if(party < dst)
                ios[dst]->send_data(data, len);
            else
                ios2[dst]->send_data(data, len);
            sent[dst] = true;
        }
#ifdef __MORE_FLUSH
        flush(dst);
#endif
    }
    void recv_data(int src, void * data, size_t len) {
        if(src != 0 and src!= party) {
            if(sent[src])flush(src);
            if(src < party)
                ios[src]->recv_data(data, len);
            else
                ios2[src]->recv_data(data, len);
        }
    }
    IOChannel& get(size_t idx, bool b = false){
        if (b)
            return *ios[idx];
        else return *ios2[idx];
    }
    void flush(int idx = 0) {
        if(idx == 0) {
            for(int i = 1; i <= nP; ++i)
                if(i != party) {
                    ios[i]->flush();
                    ios2[i]->flush();
                }
        } else {
            if(party < idx)
                ios[idx]->flush();
            else
                ios2[idx]->flush();
        }
    }
    void sync() {
        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if(i < j) {
            if(i == party) {
                ios[j]->sync();
                ios2[j]->sync();
            } else if(j == party) {
                ios[i]->sync();
                ios2[i]->sync();
            }
        }
    }
};
#endif //NETIOMP_H
