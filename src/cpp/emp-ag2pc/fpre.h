#ifndef EMP_AG2PC_FPRE_H
#define EMP_AG2PC_FPRE_H
#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>
#include "emp-ag2pc/feq.h"
#include "emp-ag2pc/helper.h"
#include "emp-ag2pc/leaky_deltaot.h"
#include "emp-ag2pc/config.h"

namespace emp {
//#define __debug

class Fpre {
    public:
        IOChannel io;
        int batch_size = 0, bucket_size = 0, size = 0;
        int party;
        block * keys = nullptr;
        bool * values = nullptr;
        PRG prg;
        PRP prp;
        PRP *prps;
        LeakyDeltaOT *abit1, *abit2;
        block Delta;
        block ZDelta;
        block one;
        Feq *eq[2];
        block * MAC = nullptr, *KEY = nullptr;
        block * MAC_res = nullptr, *KEY_res = nullptr;
        block * pretable = nullptr;
        Fpre(IOChannel io, int in_party, int bsize = 1000): io(io) {
            prps = new PRP[2];
            this->party = in_party;

            eq[0] = new Feq(io, party);
            eq[1] = new Feq(io, party);

            abit1 = new LeakyDeltaOT(io);
            abit2 = new LeakyDeltaOT(io);

            bool tmp_s[128];
            prg.random_bool(tmp_s, 128);
            tmp_s[0] = true;
            if(party == ALICE) {
                tmp_s[1] = true;
                abit1->setup_send(tmp_s);
                io.flush();
                abit2->setup_recv();
            } else {
                tmp_s[1] = false;
                abit1->setup_recv();
                io.flush();
                abit2->setup_send(tmp_s);
            }
            io.flush();

            abit1 = new LeakyDeltaOT(io);
            abit2 = new LeakyDeltaOT(io);
            if(party == ALICE) {
                abit1->setup_send(tmp_s, abit1->k0);
                abit2->setup_recv(abit2->k0, abit2->k1);
            } else {
                abit2->setup_send(tmp_s, abit2->k0);
                abit1->setup_recv(abit1->k0, abit1->k1);
            }

            if(party == ALICE) Delta = abit1->Delta;
            else Delta = abit2->Delta;
            one = makeBlock(0, 1);
            ZDelta =  Delta  & makeBlock(0xFFFFFFFFFFFFFFFF,0xFFFFFFFFFFFFFFFE);
            set_batch_size(bsize);
        }
        int permute_batch_size;
        void set_batch_size(int size) {
            size = std::max(size, 320);
            batch_size = ((size+1)/2)*2;
            if(batch_size >= 280*1000) {
                bucket_size = 3;
                permute_batch_size = 280000;
            }
            else if(batch_size >= 3100) {
                bucket_size = 4;
                permute_batch_size = 3100;
            }
            else bucket_size = 5;

            delete[] MAC;
            delete[] KEY;

            MAC = new block[batch_size * bucket_size * 3];
            KEY = new block[batch_size * bucket_size * 3];
            MAC_res = new block[batch_size * 3];
            KEY_res = new block[batch_size * 3];
//            cout << size<<"\t"<<batch_size<<"\n";
        }
        ~Fpre() {
            delete[] MAC;
            delete[] KEY;
            delete[] MAC_res;
            delete[] KEY_res;
            delete[] prps;

            delete abit1;
            delete abit2;
            delete eq[0];
            delete eq[1];
        }
        void refill() {
            auto start_time = clock_start();

            int start = 0;
            int length = batch_size;

            generate(MAC + start * bucket_size*3, KEY + start * bucket_size*3, length * bucket_size);

            if(party == ALICE) {
                // cout <<"ABIT\t"<<time_from(start_time)<<"\n";
                start_time = clock_start();
            }

            for(int i = 0; i < 2; ++i) {
                int start = i*(batch_size/2);
                int length = batch_size/2;
                check(MAC + start * bucket_size*3, KEY + start * bucket_size*3, length * bucket_size, i);
            }
            if(party == ALICE) {
                // cout <<"check\t"<<time_from(start_time)<<"\n";
                start_time = clock_start();
            }

#ifdef __debug
            check_correctness(MAC, KEY, batch_size);
#endif
            block S = coin_tossing(prg, io, party);
            if(bucket_size > 4) {
                combine(S, 0, MAC, KEY, batch_size, bucket_size, MAC_res, KEY_res);
            } else {
                int width = min((batch_size), permute_batch_size);

                int start = 0;
                int length = min(width, batch_size);
                combine(S, 0, MAC+start*bucket_size*3, KEY+start*bucket_size*3, length, bucket_size, MAC_res+start*3, KEY_res+start*3);
            }
            if(party == ALICE) {
                // cout <<"permute\t"<<time_from(start_time)<<"\n";
                start_time = clock_start();
            }

#ifdef __debug
            check_correctness(MAC, KEY, batch_size);
#endif
            char dgst[Hash::DIGEST_SIZE];
            for(int i = 1; i < 2; ++i) {
                eq[i]->dgst(dgst);
                eq[0]->add_data(dgst, Hash::DIGEST_SIZE);
            }
            if(!eq[0]->compare()) {
                error("FEQ error\n");
            }
        }

        void generate(block * MAC, block * KEY, int length) {
            if (party == ALICE) {
                abit1->send_dot(KEY, length*3);
                abit2->recv_dot(MAC, length*3);
            } else {
                // TODO: I (Andrew) swapped the two lines below to remove a
                // deadlock after I removed threading. I suspect that's fine
                // but I need to check.
                abit1->recv_dot(MAC, length*3);
                abit2->send_dot(KEY, length*3);
            }
        }

        void check(block * MAC, block * KEY, int length, int I) {
            block * G = new block[length];
            block * C = new block[length];
            block * GR = new block[length];
            bool * d = new bool[length];
            bool * dR = new bool[length];

            for (int i = 0; i < length; ++i) {
                C[i] = KEY[3*i+1] ^ MAC[3*i+1];
                C[i] = C[i] ^ (select_mask[getLSB(MAC[3*i+1])] & Delta);
                G[i] = H2D(KEY[3*i], Delta, I);
                G[i] = G[i] ^ C[i];
            }
            if(party == ALICE) {
                io.send_data(G, sizeof(block)*length);
                io.recv_data(GR, sizeof(block)*length);
            } else {
                io.recv_data(GR, sizeof(block)*length);
                io.send_data(G, sizeof(block)*length);
            }
            io.flush();
            for(int i = 0; i < length; ++i) {
                block S = H2(MAC[3*i], KEY[3*i], I);
                S = S ^ MAC[3*i+2] ^ KEY[3*i+2];
                S = S ^ (select_mask[getLSB(MAC[3*i])] & (GR[i] ^ C[i]));
                G[i] = S ^ (select_mask[getLSB(MAC[3*i+2])] & Delta);
                d[i] = getL2SB(G[i]);
            }

            if(party == ALICE) {
                io.send_bool(d, length);
                io.recv_bool(dR,length);
            } else {
                io.recv_bool(dR, length);
                io.send_bool(d, length);
            }
            io.flush();
            for(int i = 0; i < length; ++i) {
                d[i] = d[i] != dR[i];
                if (d[i]) {
                    if(party == ALICE)
                        MAC[3*i+2] = MAC[3*i+2] ^ one;
                    else
                        KEY[3*i+2] = KEY[3*i+2] ^ ZDelta;

                    G[i] = G[i] ^ Delta;
                }
                eq[I]->add_block(G[i]);
            }
            delete[] G;
            delete[] GR;
            delete[] C;
            delete[] d;
            delete[] dR;
        }
        block H2D(block a, block b, int I) {
            block d[2];
            d[0] = a;
            d[1] = a ^ b;
            prps[I].permute_block(d, 2);
            d[0] = d[0] ^ d[1];
            return d[0] ^ b;
        }

        block H2(block a, block b, int I) {
            block d[2];
            d[0] = a;
            d[1] = b;
            prps[I].permute_block(d, 2);
            d[0] = d[0] ^ d[1];
            d[0] = d[0] ^ a;
            return d[0] ^ b;
        }

        bool getL2SB(block b) {
            unsigned char x = *((unsigned char*)&b);
            return ((x >> 1) & 0x1) == 1;
        }

        void combine(block S, int I, block * MAC, block * KEY, int length, int bucket_size, block * MAC_res, block * KEY_res) {
            int *location = new int[length*bucket_size];
            for(int i = 0; i < length*bucket_size; ++i) location[i] = i;
            PRG prg(&S, I);
            int * ind = new int[length*bucket_size];
            prg.random_data(ind, length*bucket_size*4);
            for(int i = length*bucket_size-1; i>=0; --i) {
                int index = ind[i]%(i+1);
                index = index>0? index:(-1*index);
                int tmp = location[i];
                location[i] = location[index];
                location[index] = tmp;
            }
            delete[] ind;

            bool *data = new bool[length*bucket_size];
            bool *data2 = new bool[length*bucket_size];
            for(int i = 0; i < length; ++i) {
                for(int j = 1; j < bucket_size; ++j) {
                    data[i*bucket_size+j] = getLSB(MAC[location[i*bucket_size]*3+1] ^ MAC[location[i*bucket_size+j]*3+1]);
                }
            }
            if(party == ALICE) {
                io.send_bool(data, length*bucket_size);
                io.recv_bool(data2, length*bucket_size);
            } else {
                io.recv_bool(data2, length*bucket_size);
                io.send_bool(data, length*bucket_size);
            }
            io.flush();
            for(int i = 0; i < length; ++i) {
                for(int j = 1; j < bucket_size; ++j) {
                    data[i*bucket_size+j] = (data[i*bucket_size+j] != data2[i*bucket_size+j]);
                }
            }
            for(int i = 0; i < length; ++i) {
                for(int j = 0; j < 3; ++j) {
                    MAC_res[i*3+j] = MAC[location[i*bucket_size]*3+j];
                    KEY_res[i*3+j] = KEY[location[i*bucket_size]*3+j];
                }
                for(int j = 1; j < bucket_size; ++j) {
                    MAC_res[3*i] = MAC_res[3*i] ^ MAC[location[i*bucket_size+j]*3];
                    KEY_res[3*i] = KEY_res[3*i] ^ KEY[location[i*bucket_size+j]*3];

                    MAC_res[i*3+2] = MAC_res[i*3+2] ^ MAC[location[i*bucket_size+j]*3+2];
                    KEY_res[i*3+2] = KEY_res[i*3+2] ^ KEY[location[i*bucket_size+j]*3+2];

                    if(data[i*bucket_size+j]) {
                        KEY_res[i*3+2] = KEY_res[i*3+2] ^ KEY[location[i*bucket_size+j]*3];
                        MAC_res[i*3+2] = MAC_res[i*3+2] ^ MAC[location[i*bucket_size+j]*3];
                    }
                }
            }

            delete[] data;
            delete[] location;
            delete[] data2;
        }

//for debug
        void check_correctness(block * MAC, block * KEY, int length) {
            if (party == ALICE) {
                for(int i = 0; i < length*3; ++i) {
                    bool tmp = getLSB(MAC[i]);
                    io.send_data(&tmp, 1);
                }
                io.send_block(&Delta, 1);
                io.send_block(KEY, length*3);
                block DD;
                io.recv_block(&DD, 1);

                for(int i = 0; i < length*3; ++i) {
                    block tmp;
                    io.recv_block(&tmp, 1);
                    if(getLSB(MAC[i])) tmp = tmp ^ DD;
                    if (!cmpBlock(&tmp, &MAC[i], 1))
                        throw std::runtime_error(std::to_string(i) + " WRONG ABIT2!");
                }

            } else {
                bool tmp[3];
                for(int i = 0; i < length; ++i) {
                    io.recv_data(tmp, 3);
                    bool res = ((tmp[0] != getLSB(MAC[3*i]) ) && (tmp[1] != getLSB(MAC[3*i+1])));
                    if(res != (tmp[2] != getLSB(MAC[3*i+2])) ) {
                        throw std::runtime_error(std::to_string(i) + " WRONG!");
                    }
                }
                block DD;
                io.recv_block(&DD, 1);

                for(int i = 0; i < length*3; ++i) {
                    block tmp;
                    io.recv_block(&tmp, 1);
                    if(getLSB(MAC[i])) tmp = tmp ^ DD;
                    if (!cmpBlock(&tmp, &MAC[i], 1))
                        throw std::runtime_error(std::to_string(i) + " WRONG ABIT2!");
                }

                io.send_block(&Delta, 1);
                io.send_block(KEY, length*3);
            }
            io.flush();
        }
};
}
#endif// FPRE_H
