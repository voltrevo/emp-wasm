#ifndef EMP_AG2PC_HELPER_H
#define EMP_AG2PC_HELPER_H
#include <emp-tool/emp-tool.h>
#include "config.h"

using std::cout;
using std::endl;

namespace emp {

template<int B>
void send_partial_block(IOChannel& io, const block * data, int length) {
    for(int i = 0; i < length; ++i) {
        io.send_data(&(data[i]), B);
    }
}

template<int B>
void recv_partial_block(IOChannel& io, block * data, int length) {
    for(int i = 0; i < length; ++i) {
        io.recv_data(&(data[i]), B);
    }
}

block coin_tossing(PRG prg, IOChannel& io, int party) {
    block S, S2;
    char dgst[Hash::DIGEST_SIZE];
    prg.random_block(&S, 1);
    if(party == ALICE) {
        Hash::hash_once(dgst, &S, sizeof(block));
        io.send_data(dgst, Hash::DIGEST_SIZE);
        io.recv_block(&S2, 1);
        io.send_block(&S, 1);
    } else {
        char dgst2[Hash::DIGEST_SIZE];
        io.recv_data(dgst2, Hash::DIGEST_SIZE);
        io.send_block(&S, 1);
        io.recv_block(&S2, 1);
        Hash::hash_once(dgst, &S2, sizeof(block));
        if (memcmp(dgst, dgst2, Hash::DIGEST_SIZE)!= 0)
            error("cheat CT!");
    }
    io.flush();
    return S ^ S2;
}

}
#endif// __HELPER
