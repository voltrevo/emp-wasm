#ifndef ABIT_MP_H
#define ABIT_MP_H

#include <optional>

#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>
#include "netmp.h"
#include "helper.h"
#include "nvec.h"
#include "vec.h"

class ABitMP { public:
    std::shared_ptr<IMultiIO> io;
    int nP;
    Vec<std::optional<IKNP>> abit1;
    Vec<std::optional<IKNP>> abit2;
    int party;
    PRG prg;
    block Delta;
    Hash hash;
    int ssp;
    block * pretable;

    ABitMP(
        std::shared_ptr<IMultiIO>& io,
        bool * _tmp = nullptr,
        int ssp = 40
    ):
        io(io),
        nP(io->size()),
        abit1(nP+1),
        abit2(nP+1)
    {
        this->ssp = ssp;
        this->party = io->party();
        bool tmp[128];
        if(_tmp == nullptr) {
            prg.random_bool(tmp, 128);
        } else {
            memcpy(tmp, _tmp, 128);
        }

        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if(i < j) {
            if(i == party) {
                abit1[j].emplace(get_recv_channel(*io, j));
                abit2[j].emplace(get_send_channel(*io, j));
            } else if (j == party) {
                abit2[i].emplace(get_send_channel(*io, i));
                abit1[i].emplace(get_recv_channel(*io, i));
            }
        }

        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if(i < j) {
            if(i == party) {
                abit1[j]->setup_send(tmp);
                io->flush(j);

                abit2[j]->setup_recv();
                io->flush(j);
            } else if (j == party) {
                abit2[i]->setup_recv();
                io->flush(i);

                abit1[i]->setup_send(tmp);
                io->flush(i);
            }
        }

        if(party == 1)
            Delta = abit1[2]->Delta;
        else
            Delta = abit1[1]->Delta;
    }

    void compute(NVec<block>& MAC, NVec<block>& KEY, bool* data, int length) {
        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            if (party < party2) {
                abit2[party2]->recv_cot(&MAC.at(party2, 0), data, length);
                io->flush(party2);

                abit1[party2]->send_cot(&KEY.at(party2, 0), length);
                io->flush(party2);
            } else {
                abit1[party2]->send_cot(&KEY.at(party2, 0), length);
                io->flush(party2);

                abit2[party2]->recv_cot(&MAC.at(party2, 0), data, length);
                io->flush(party2);
            }
        }
#ifdef __debug
        check_MAC(nP, *io, MAC, KEY, data, Delta, length, party);
#endif
    }

    void check(const NVec<block>& MAC, const NVec<block>& KEY, bool* data, int length) {
        check1(MAC, KEY, data, length);
        check2(MAC, KEY, data, length);
    }

    void check1(const NVec<block>& MAC, const NVec<block>& KEY, bool* data, int length) {
        block seed = sampleRandom(nP, *io, &prg, party);
        PRG prg2(&seed);
        uint8_t * tmp;
        NVec<block> Ms(nP+1, ssp);
        NVec<bool> bs(nP+1, ssp);
        NVec<block> Ks(nP+1, ssp);
        NVec<block> tMs(nP+1, ssp);
        NVec<bool> tbs(nP+1, ssp);

        tmp = new uint8_t[ssp*length];
        prg2.random_data(tmp, ssp*length);
        for(int i = 0; i < ssp*length; ++i)
            tmp[i] = tmp[i] % 4;
//        for(int j = 0; j < ssp; ++j) {
//            tmp[j] = new bool[length];
//            for(int k = 0; k < ssp; ++k)
//                tmp[j][length - ssp + k] = (k == j);
//        }

        const int chk = 1;
        const int SIZE = 1024*2;
        block (* tMAC)[4] = new block[SIZE/chk][4];
        block (* tKEY)[4] = new block[SIZE/chk][4];
        bool (* tb)[4] = new bool[length/chk][4];
        memset(tMAC, 0, sizeof(block)*4*SIZE/chk);
        memset(tKEY, 0, sizeof(block)*4*SIZE/chk);
        memset(tb, false, 4*length/chk);
        for(int i = 0; i < length; i+=chk) {
            tb[i/chk][1] = data[i];
            tb[i/chk][2] = data[i+1];
            tb[i/chk][3] = data[i] != data[i+1];
        }

        for(int k = 1; k <= nP; ++k) if(k != party) {
            uint8_t * tmpptr = tmp;
            for(int tt = 0; tt < length/SIZE; tt++) {
                int start = SIZE*tt;
                for(int i = SIZE*tt; i < SIZE*(tt+1) and i < length; i+=chk) {
                  tMAC[(i-start)/chk][1] = MAC.at(k, i);
                  tMAC[(i-start)/chk][2] = MAC.at(k, i+1);
                  tMAC[(i-start)/chk][3] = MAC.at(k, i) ^ MAC.at(k, i+1);

                  tKEY[(i-start)/chk][1] = KEY.at(k, i);
                  tKEY[(i-start)/chk][2] = KEY.at(k, i+1);
                  tKEY[(i-start)/chk][3] = KEY.at(k, i) ^ KEY.at(k, i+1);
                  for(int j = 0; j < ssp; ++j) {
                             Ms.at(k, j) = Ms.at(k, j) ^ tMAC[(i-start)/chk][*tmpptr];
                             Ks.at(k, j) = Ks.at(k, j) ^ tKEY[(i-start)/chk][*tmpptr];
                             bs.at(k, j) = bs.at(k, j) != tb[i/chk][*tmpptr];
                             ++tmpptr;
                    }
                }
            }
        }
        delete[] tmp;
        vector<bool> res;
        //TODO: they should not need to send MACs.
        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            get_send_channel(*io, party2).send_data(&Ms.at(party2, 0), sizeof(block)*ssp);
            get_send_channel(*io, party2).send_data(&bs.at(party2, 0), ssp);
            io->flush(party2);
            res.push_back(false);

            get_recv_channel(*io, party2).recv_data(&tMs.at(party2, 0), sizeof(block)*ssp);
            get_recv_channel(*io, party2).recv_data(&tbs.at(party2, 0), ssp);
            for(int k = 0; k < ssp; ++k) {
                if(tbs.at(party2, k))
                    Ks.at(party2, k) = Ks.at(party2, k) ^ Delta;
            }
            res.push_back(!cmpBlock(&Ks.at(party2, 0), &tMs.at(party2, 0), ssp));
        }
        if(checkCheat(res)) error("cheat check1\n");
    }

    void check2(const NVec<block>& MAC, const NVec<block> KEY, bool* data, int length) {
        //last 2*ssp are garbage already.
        NVec<block> Ks(2, ssp);
        NVec<block> Ms(nP+1, nP+1, ssp);
        NVec<block> KK(nP+1, ssp);
        NVec<bool> bs(nP+1, ssp);

        char (*dgst)[Hash::DIGEST_SIZE] = new char[nP+1][Hash::DIGEST_SIZE];
        char (*dgst0)[Hash::DIGEST_SIZE] = new char[ssp*(nP+1)][Hash::DIGEST_SIZE];
        char (*dgst1)[Hash::DIGEST_SIZE] = new char[ssp*(nP+1)][Hash::DIGEST_SIZE];

        for(int i = 0; i < ssp; ++i) {
            Ks.at(0, i) = zero_block;
            for(int j = 1; j <= nP; ++j) if(j != party)
                Ks.at(0, i) = Ks.at(0, i) ^ KEY.at(j, length-3*ssp+i);

            Ks.at(1, i) = Ks.at(0, i) ^ Delta;
            Hash::hash_once(dgst0[party*ssp+i], &Ks.at(0, i), sizeof(block));
            Hash::hash_once(dgst1[party*ssp+i], &Ks.at(1, i), sizeof(block));
        }
        Hash h;
        h.put(data+length-3*ssp, ssp);
        for(int j = 1; j <= nP; ++j) if(j != party) {
            h.put(&MAC.at(j, length-3*ssp), ssp*sizeof(block));
        }
        h.digest(dgst[party]);

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;
            get_send_channel(*io, party2).send_data(dgst[party], Hash::DIGEST_SIZE);
            get_send_channel(*io, party2).send_data(dgst0[party*ssp], Hash::DIGEST_SIZE*ssp);
            get_send_channel(*io, party2).send_data(dgst1[party*ssp], Hash::DIGEST_SIZE*ssp);
            get_recv_channel(*io, party2).recv_data(dgst[party2], Hash::DIGEST_SIZE);
            get_recv_channel(*io, party2).recv_data(dgst0[party2*ssp], Hash::DIGEST_SIZE*ssp);
            get_recv_channel(*io, party2).recv_data(dgst1[party2*ssp], Hash::DIGEST_SIZE*ssp);
        }

        vector<bool> res2;
        for(int k = 1; k <= nP; ++k) if(k!= party)
            memcpy(&Ms.at(party, k, 0), &MAC.at(k, length-3*ssp), sizeof(block)*ssp);

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            get_send_channel(*io, party2).send_data(data + length - 3*ssp, ssp);
            for(int k = 1; k <= nP; ++k) if(k != party)
                get_send_channel(*io, party2).send_data(&MAC.at(k, length - 3*ssp), sizeof(block)*ssp);
            res2.push_back(false);

            Hash h;
            get_recv_channel(*io, party2).recv_data(&bs.at(party2, 0), ssp);
            h.put(&bs.at(party2, 0), ssp);
            for(int k = 1; k <= nP; ++k) if(k != party2) {
                get_recv_channel(*io, party2).recv_data(&Ms.at(party2, k, 0), sizeof(block)*ssp);
                h.put(&Ms.at(party2, k, 0), sizeof(block)*ssp);
            }
            char tmp[Hash::DIGEST_SIZE];h.digest(tmp);
            res2.push_back(strncmp(tmp, dgst[party2], Hash::DIGEST_SIZE) != 0);
        }
        if(checkCheat(res2)) error("commitment 1\n");

        memset(&bs.at(party, 0), false, ssp);
        for(int i = 1; i <= nP; ++i) if(i != party) {
            for(int j = 0; j < ssp; ++j)
                bs.at(party, j) = bs.at(party, j) != bs.at(i, j);
        }
        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            get_send_channel(*io, party2).send_data(&bs.at(party, 0), ssp);
            for(int i = 0; i < ssp; ++i) {
                if (bs.at(party, i))
                    get_send_channel(*io, party2).send_data(&Ks.at(1, i), sizeof(block));
                else
                    get_send_channel(*io, party2).send_data(&Ks.at(0, i), sizeof(block));
            }
            io->flush(party2);
            res2.push_back(false);

            bool cheat = false;
            bool *tmp_bool = new bool[ssp];
            get_recv_channel(*io, party2).recv_data(tmp_bool, ssp);
            get_recv_channel(*io, party2).recv_data(&KK.at(party2, 0), ssp*sizeof(block));
            for(int i = 0; i < ssp; ++i) {
                char tmp[Hash::DIGEST_SIZE];
                Hash::hash_once(tmp, &KK.at(party2, i), sizeof(block));
                if(tmp_bool[i])
                    cheat = cheat or (strncmp(tmp, dgst1[party2*ssp+i], Hash::DIGEST_SIZE)!=0);
                else
                    cheat = cheat or (strncmp(tmp, dgst0[party2*ssp+i], Hash::DIGEST_SIZE)!=0);
            }
            delete[] tmp_bool;
            res2.push_back(cheat);
        }
        if(checkCheat(res2)) error("commitments 2\n");

        bool cheat = false;
        block *tmp_block = new block[ssp];
        for(int i = 1; i <= nP; ++i) if (i != party) {
            memset(tmp_block, 0, sizeof(block)*ssp);
            for(int j = 1; j <= nP; ++j) if(j != i) {
                for(int k = 0; k < ssp; ++k)
                    tmp_block[k] = tmp_block[k] ^ Ms.at(j, i, k);
            }
            cheat = cheat or !cmpBlock(tmp_block, &KK.at(i, 0), ssp);
        }
        if(cheat) error("cheat aShare\n");

        delete[] dgst;
        delete[] dgst0;
        delete[] dgst1;
        delete[] tmp_block;
    }
};
#endif //ABIT_MP_H
