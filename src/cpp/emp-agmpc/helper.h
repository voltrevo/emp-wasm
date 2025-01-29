#ifndef __HELPER
#define __HELPER
#include <emp-tool/emp-tool.h>
#include "cmpc_config.h"
#include "netmp.h"
#include "nvec.h"
#include <future>
using namespace emp;
using std::future;
using std::cout;
using std::max;
using std::cerr;
using std::endl;
using std::flush;

const static block inProdTableBlock[] = {zero_block, all_one_block};

block inProd(bool * b, block * blk, int length) {
        block res = zero_block;
        for(int i = 0; i < length; ++i)
//            if(b[i])
//                res = res ^ blk[i];
            res = res ^ (inProdTableBlock[b[i]] & blk[i]);
        return res;
}
#ifdef __GNUC__
    #ifndef __clang__
        #pragma GCC push_options
        #pragma GCC optimize ("unroll-loops")
    #endif
#endif

template<int ssp>
void inProdhelp(block *Ms,  bool * tmp[ssp],  block * MAC, int i) {
    for(int j = 0; j < ssp; ++j)
        Ms[j] = Ms[j] ^ (inProdTableBlock[tmp[j][i]] & MAC[i]);
}
#ifdef __GNUC__
    #ifndef __clang__
        #pragma GCC pop_options
    #endif
#endif


template<int ssp>
void inProds(block *Ms,  bool * tmp[ssp], block * MAC, int length) {
    memset(Ms, 0, sizeof(block)*ssp);
    for(int i = 0; i < length; ++i)  {
        inProdhelp<ssp>(Ms, tmp, MAC, i);
    }
}
bool inProd(bool * b, bool* b2, int length) {
        bool res = false;
        for(int i = 0; i < length; ++i)
            res = (res != (b[i] and b2[i]));
        return res;
}

template<typename T>
void joinNclean(vector<future<T>>& res) {
    for(auto &v: res) v.get();
    res.clear();
}

bool checkCheat(vector<bool>& res) {
    bool cheat = false;
    for(auto v: res) cheat = cheat or v;
    return cheat;
}

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

block sampleRandom(int nP, NetIOMP * io, PRG * prg, int party) {
    vector<bool> res2;
    char (*dgst)[Hash::DIGEST_SIZE] = new char[nP+1][Hash::DIGEST_SIZE];
    block *S = new block[nP+1];
    prg->random_block(&S[party], 1);
    Hash::hash_once(dgst[party], &S[party], sizeof(block));

    for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
        int party2 = i + j - party;
        io->send_channel(party2).send_data(dgst[party], Hash::DIGEST_SIZE);
        io->recv_channel(party2).recv_data(dgst[party2], Hash::DIGEST_SIZE);
    }
    for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
        int party2 = i + j - party;
        io->send_channel(party2).send_data(&S[party], sizeof(block));
        io->recv_channel(party2).recv_data(&S[party2], sizeof(block));
        char tmp[Hash::DIGEST_SIZE];
        Hash::hash_once(tmp, &S[party2], sizeof(block));
        bool cheat = strncmp(tmp, dgst[party2], Hash::DIGEST_SIZE)!=0;
        res2.push_back(cheat);
    }
    bool cheat = checkCheat(res2);
    if(cheat) {
        cout <<"cheat in sampleRandom\n"<<flush;
        exit(0);
    }
    for(int i = 2; i <= nP; ++i)
        S[1] = S[1] ^ S[i];
    block result = S[1];
    delete[] S;
    delete[] dgst;
    return result;
}

void check_MAC(int nP, NetIOMP * io, const NVec<block>& MAC, const NVec<block>& KEY, bool * r, block Delta, int length, int party) {
    block * tmp = new block[length];
    block tD;
    for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if (i < j) {
        if(party == i) {
            io->send_channel(j).send_data(&Delta, sizeof(block));
            io->send_channel(j).send_data(&KEY.at(j, 0), sizeof(block)*length);
        } else if(party == j) {
            io->recv_channel(i).recv_data(&tD, sizeof(block));
            io->recv_channel(i).recv_data(tmp, sizeof(block)*length);
            for(int k = 0; k < length; ++k) {
                if(r[k])tmp[k] = tmp[k] ^ tD;
            }
            if(!cmpBlock(&MAC.at(i, 0), tmp, length))
                error("check_MAC failed!");
        }
    }
    delete[] tmp;
    if(party == 1)
        cerr<<"check_MAC pass!\n"<<flush;
}

void check_correctness(int nP, NetIOMP* io, bool * r, int length, int party) {
    if (party == 1) {
        bool * tmp1 = new bool[length*3];
        bool * tmp2 = new bool[length*3];
        memcpy(tmp1, r, length*3);
        for(int i = 2; i <= nP; ++i) {
            io->recv_channel(i).recv_data(tmp2, length*3);
            for(int k = 0; k < length*3; ++k)
                tmp1[k] = (tmp1[k] != tmp2[k]);
        }
        for(int k = 0; k < length; ++k) {
            if((tmp1[3*k] and tmp1[3*k+1]) != tmp1[3*k+2])
                error("check_correctness failed!");
        }
        delete[] tmp1;
        delete[] tmp2;
        cerr<<"check_correctness pass!\n"<<flush;
    } else {
        io->send_channel(1).send_data(r, length*3);
    }
}

uint64_t count_multi_io(IMultiIO& io) {
    uint64_t res = 0;

    for (int i = 1; i <= io.size(); ++i) {
        if (i != io.party()) {
            res += *io.send_channel(i).counter;
            res += *io.recv_channel(i).counter;
        }
    }

    return res;
}

inline const char* hex_char_to_bin(char c) {
    switch(toupper(c)) {
        case '0': return "0000";
        case '1': return "0001";
        case '2': return "0010";
        case '3': return "0011";
        case '4': return "0100";
        case '5': return "0101";
        case '6': return "0110";
        case '7': return "0111";
        case '8': return "1000";
        case '9': return "1001";
        case 'A': return "1010";
        case 'B': return "1011";
        case 'C': return "1100";
        case 'D': return "1101";
        case 'E': return "1110";
        case 'F': return "1111";
        default: return "0";
    }
}


inline std::string hex_to_binary(std::string hex) {
    std::string bin;
    for(unsigned i = 0; i != hex.length(); ++i)
        bin += hex_char_to_bin(hex[i]);
    return bin;
}

#endif// __HELPER
