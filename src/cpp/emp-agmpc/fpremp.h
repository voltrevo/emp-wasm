//TODO: check MACs
#ifndef FPRE_MP_H
#define FPRE_MP_H
#include <emp-tool/emp-tool.h>
#include <thread>
#include "abitmp.h"
#include "netmp.h"
#include "cmpc_config.h"
#include "nvec.h"

using namespace emp;

class FpreMP { public:
    int nP;
    int party;
    std::shared_ptr<IMultiIO> io;
    ABitMP* abit;
    block Delta;
    CRH * prps;
    CRH * prps2;
    PRG * prgs;
    PRG prg;
    int ssp;

    FpreMP(
        std::shared_ptr<IMultiIO>& io,
        bool * _delta = nullptr,
        int ssp = 40
    ):
        io(io),
        nP(io->size()),
        party(io->party())
    {
        this ->ssp = ssp;
        abit = new ABitMP(io, _delta, ssp);
        Delta = abit->Delta;
        prps = new CRH[nP+1];
        prps2 = new CRH[nP+1];
        prgs = new PRG[nP+1];
    }
    ~FpreMP(){
        delete[] prps;
        delete[] prps2;
        delete[] prgs;
        delete abit;
    }
    int get_bucket_size(int size) {
        size = max(size, 320);
        int batch_size = ((size+2-1)/2)*2;
        if(batch_size >= 280*1000)
            return 3;
        else if(batch_size >= 3100)
            return 4;
        else return 5;
    }
    void compute(NVec<block>& MAC, NVec<block>& KEY, bool* r, int length) {
        int64_t bucket_size = get_bucket_size(length);
        NVec<block> tMAC(nP+1, length*bucket_size*3+3*ssp);
        NVec<block> tKEY(nP+1, length*bucket_size*3+3*ssp);
        NVec<block> tKEYphi(nP+1, length*bucket_size*3+3*ssp);
        NVec<block> tMACphi(nP+1, length*bucket_size*3+3*ssp);
        Vec<block> phi(length*bucket_size);
        NVec<block> X(nP+1, ssp);
        Vec<bool> tr(length*bucket_size*3+3*ssp);
        NVec<bool> s(nP+1, length*bucket_size);
        Vec<bool> e(length*bucket_size);

        prg.random_bool(&tr[0], length*bucket_size*3+3*ssp);
        // memset(tr, false, length*bucket_size*3+3*ssp);
        abit->compute(tMAC, tKEY, &tr[0], length*bucket_size*3 + 3*ssp);

        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if (i < j ) {
            if(i == party) {
                prgs[j].random_bool(&s.at(j, 0), length*bucket_size);
                for(int k = 0; k < length*bucket_size; ++k) {
                    uint8_t data = garble(&tKEY.at(j, 0), &tr[0], &s.at(j, 0), k, j);
                    io->send_channel(j).send_data(&data, 1);
                    s.at(j, k) = (s.at(j, k) != (tr[3*k] and tr[3*k+1]));
                }
                io->flush(j);
            } else if (j == party) {
                for(int k = 0; k < length*bucket_size; ++k) {
                    uint8_t data = 0;
                    io->recv_channel(i).recv_data(&data, 1);
                    bool tmp = evaluate(data, &tMAC.at(i, 0), &tr[0], k, i);
                    s.at(i, k) = (tmp != (tr[3*k] and tr[3*k+1]));
                }
            }
        }
        for(int k = 0; k < length*bucket_size; ++k) {
            s.at(0, k) = (tr[3*k] and tr[3*k+1]);
            for(int i = 1; i <= nP; ++i)
                if (i != party) {
                    s.at(0, k) = (s.at(0, k) != s.at(i, k));
                }
            e[k] = (s.at(0, k) != tr[3*k+2]);
            tr[3*k+2] = s.at(0, k);
        }

#ifdef __debug
        check_correctness(nP, *io, &tr[0], length*bucket_size, party);
#endif
        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            io->send_channel(party2).send_data(&e[0], length*bucket_size);
            io->flush(party2);

            bool * tmp = new bool[length*bucket_size];
            io->recv_channel(party2).recv_data(tmp, length*bucket_size);
            for(int k = 0; k < length*bucket_size; ++k) {
                if(tmp[k])
                    tKEY.at(party2, 3*k+2) = tKEY.at(party2, 3*k+2) ^ Delta;
            }
            delete[] tmp;
        }
#ifdef __debug
        check_MAC(nP, *io, tMAC, tKEY, &tr[0], Delta, length*bucket_size*3, party);
#endif
        abit->check(tMAC, tKEY, &tr[0], length*bucket_size*3 + 3*ssp);
        //check compute phi
        for(int k = 0; k < length*bucket_size; ++k) {
            phi[k] = zero_block;
            for(int i = 1; i <= nP; ++i) if (i != party) {
                phi[k] = phi[k] ^ tKEY.at(i, 3*k+1);
                phi[k] = phi[k] ^ tMAC.at(i, 3*k+1);
            }
            if(tr[3*k+1])phi[k] = phi[k] ^ Delta;
        }

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            if (party < party2) {
                {
                    block bH[2], tmpH[2];
                    for(int k = 0; k < length*bucket_size; ++k) {
                        bH[0] = tKEY.at(party2, 3*k);
                        bH[1] = bH[0] ^ Delta;
                        HnID(prps+party2, bH, bH, 2*k, 2, tmpH);
                        tKEYphi.at(party2, k) = bH[0];
                        bH[1] = bH[0] ^ bH[1];
                        bH[1] = phi[k] ^ bH[1];
                        io->send_channel(party2).send_data(&bH[1], sizeof(block));
                    }
                    io->flush(party2);
                }

                {
                    block bH;
                    for(int k = 0; k < length*bucket_size; ++k) {
                        io->recv_channel(party2).recv_data(&bH, sizeof(block));
                        block hin = sigma(tMAC.at(party2, 3*k)) ^ makeBlock(0, 2*k+tr[3*k]);
                        tMACphi.at(party2, k) = prps2[party2].H(hin);
                        if(tr[3*k])tMACphi.at(party2, k) = tMACphi.at(party2, k) ^ bH;
                    }
                }
            } else {
                {
                    block bH;
                    for(int k = 0; k < length*bucket_size; ++k) {
                        io->recv_channel(party2).recv_data(&bH, sizeof(block));
                        block hin = sigma(tMAC.at(party2, 3*k)) ^ makeBlock(0, 2*k+tr[3*k]);
                        tMACphi.at(party2, k) = prps2[party2].H(hin);
                        if(tr[3*k])tMACphi.at(party2, k) = tMACphi.at(party2, k) ^ bH;
                    }
                }

                {
                    block bH[2], tmpH[2];
                    for(int k = 0; k < length*bucket_size; ++k) {
                        bH[0] = tKEY.at(party2, 3*k);
                        bH[1] = bH[0] ^ Delta;
                        HnID(prps+party2, bH, bH, 2*k, 2, tmpH);
                        tKEYphi.at(party2, k) = bH[0];
                        bH[1] = bH[0] ^ bH[1];
                        bH[1] = phi[k] ^ bH[1];
                        io->send_channel(party2).send_data(&bH[1], sizeof(block));
                    }
                    io->flush(party2);
                }
            }
        }

        bool * xs = new bool[length*bucket_size];
        for(int i = 0; i < length*bucket_size; ++i) xs[i] = tr[3*i];

#ifdef __debug
        check_MAC_phi(tMACphi, tKEYphi, &phi[0], xs, length*bucket_size);
#endif
        //tKEYphti use as H
        for(int k = 0; k < length*bucket_size; ++k) {
            tKEYphi.at(party, k) = zero_block;
            for(int i = 1; i <= nP; ++i) if (i != party) {
                tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ tKEYphi.at(i, k);
                tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ tMACphi.at(i, k);
                tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ tKEY.at(i, 3*k+2);
                tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ tMAC.at(i, 3*k+2);
            }
            if(tr[3*k])     tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ phi[k];
            if(tr[3*k+2])tKEYphi.at(party, k) = tKEYphi.at(party, k) ^ Delta;
        }

#ifdef __debug
        check_zero(&tKEYphi.at(party, 0), length*bucket_size);
#endif

        block prg_key = sampleRandom(nP, *io, &prg, party);
        PRG prgf(&prg_key);
        char (*dgst)[Hash::DIGEST_SIZE] = new char[nP+1][Hash::DIGEST_SIZE];
        bool * tmp = new bool[length*bucket_size];
        for(int i = 0; i < ssp; ++i) {
            prgf.random_bool(tmp, length*bucket_size);
            X.at(party, i) = inProd(tmp, &tKEYphi.at(party, 0), length*bucket_size);
        }
        Hash::hash_once(dgst[party], &X.at(party, 0), sizeof(block)*ssp);

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;
            io->send_channel(party2).send_data(dgst[party], Hash::DIGEST_SIZE);
            io->recv_channel(party2).recv_data(dgst[party2], Hash::DIGEST_SIZE);
        }

        vector<bool> res2;

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;
            io->send_channel(party2).send_data(&X.at(party, 0), sizeof(block)*ssp);
            io->recv_channel(party2).recv_data(&X.at(party2, 0), sizeof(block)*ssp);
            char tmp[Hash::DIGEST_SIZE];
            Hash::hash_once(tmp, &X.at(party2, 0), sizeof(block)*ssp);
            res2.push_back(strncmp(tmp, dgst[party2], Hash::DIGEST_SIZE)!=0);
        }
        if(checkCheat(res2)) error("commitment");

        for(int i = 2; i <= nP; ++i)
            xorBlocks_arr(&X.at(1, 0), &X.at(1, 0), &X.at(i, 0), ssp);
        for(int i = 0; i < ssp; ++i)X.at(2, i) = zero_block;
        if(!cmpBlock(&X.at(1, 0), &X.at(2, 0), ssp)) error("AND check");

        //land -> and
        block S = sampleRandom(nP, *io, &prg, party);

        int * ind = new int[length*bucket_size];
        int *location = new int[length*bucket_size];
        bool * d[nP+1];
        for(int i = 1; i <= nP; ++i)
            d[i] = new bool[length*(bucket_size-1)];
        for(int i = 0; i < length*bucket_size; ++i)
            location[i] = i;
        PRG prg2(&S);
        prg2.random_data(ind, length*bucket_size*4);
        for(int i = length*bucket_size-1; i>=0; --i) {
            int index = ind[i]%(i+1);
            index = index>0? index:(-1*index);
            int tmp = location[i];
            location[i] = location[index];
            location[index] = tmp;
        }
        delete[] ind;

        for(int i = 0; i < length; ++i) {
            for(int j = 0; j < bucket_size-1; ++j)
                d[party][(bucket_size-1)*i+j] = tr[3*location[i*bucket_size]+1] != tr[3*location[i*bucket_size+1+j]+1];
            for(int j = 1; j <= nP; ++j) if (j!= party) {
                memcpy(&MAC.at(j, 3*i), &tMAC.at(j, 3*location[i*bucket_size]), 3*sizeof(block));
                memcpy(&KEY.at(j, 3*i), &tKEY.at(j, 3*location[i*bucket_size]), 3*sizeof(block));
                for(int k = 1; k < bucket_size; ++k) {
                    MAC.at(j, 3*i) = MAC.at(j, 3*i) ^ tMAC.at(j, 3*location[i*bucket_size+k]);
                    KEY.at(j, 3*i) = KEY.at(j, 3*i) ^ tKEY.at(j, 3*location[i*bucket_size+k]);

                    MAC.at(j, 3*i+2) = MAC.at(j, 3*i+2) ^ tMAC.at(j, 3*location[i*bucket_size+k]+2);
                    KEY.at(j, 3*i+2) = KEY.at(j, 3*i+2) ^ tKEY.at(j, 3*location[i*bucket_size+k]+2);
                }
            }
            memcpy(&r[3*i], &tr[3*location[i*bucket_size]], 3);
            for(int k = 1; k < bucket_size; ++k) {
                r[3*i] = r[3*i] != tr[3*location[i*bucket_size+k]];
                r[3*i+2] = r[3*i+2] != tr[3*location[i*bucket_size+k]+2];
            }
        }

        for(int i = 1; i <= nP; ++i) for(int j = 1; j<= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;
            io->send_channel(party2).send_data(d[party], (bucket_size-1)*length);
            io->flush(party2);
            io->recv_channel(party2).recv_data(d[party2], (bucket_size-1)*length);
        }
        for(int i = 2; i <= nP; ++i)
            for(int j = 0; j <  (bucket_size-1)*length; ++j)
                d[1][j] = d[1][j]!=d[i][j];

        for(int i = 0; i < length; ++i)  {
            for(int j = 1; j <= nP; ++j)if (j!= party) {
                for(int k = 1; k < bucket_size; ++k)
                    if(d[1][(bucket_size-1)*i+k-1]) {
                        MAC.at(j, 3*i+2) = MAC.at(j, 3*i+2) ^ tMAC.at(j, 3*location[i*bucket_size+k]);
                        KEY.at(j, 3*i+2) = KEY.at(j, 3*i+2) ^ tKEY.at(j, 3*location[i*bucket_size+k]);
                    }
            }
            for(int k = 1; k < bucket_size; ++k)
                if(d[1][(bucket_size-1)*i+k-1]) {
                    r[3*i+2] = r[3*i+2] != tr[3*location[i*bucket_size+k]];
                }
        }

#ifdef __debug
        check_MAC(nP, *io, MAC, KEY, r, Delta, length*3, party);
        check_correctness(nP, *io, r, length, party);
#endif

//        ret.get();
    }

    //TODO: change to justGarble
    uint8_t garble(block * KEY, bool * r, bool * r2, int i, int I) {
        uint8_t data = 0;
        block tmp[4], tmp2[4], tmpH[4];
        tmp[0] = KEY[3*i];
        tmp[1] = tmp[0] ^ Delta;
        tmp[2] = KEY[3*i+1];
        tmp[3] = tmp[2] ^ Delta;
        HnID(prps+I, tmp, tmp, 4*i, 4, tmpH);

        tmp2[0] = tmp[0] ^ tmp[2];
        tmp2[1] = tmp[1] ^ tmp[2];
        tmp2[2] = tmp[0] ^ tmp[3];
        tmp2[3] = tmp[1] ^ tmp[3];

        data = getLSB(tmp2[0]);
        data |= (getLSB(tmp2[1])<<1);
        data |= (getLSB(tmp2[2])<<2);
        data |= (getLSB(tmp2[3])<<3);
        if ( ((false != r[3*i] ) && (false != r[3*i+1])) != r2[i] )
            data= data ^ 0x1;
        if ( ((true != r[3*i] ) && (false != r[3*i+1])) != r2[i] )
            data = data ^ 0x2;
        if ( ((false != r[3*i] ) && (true != r[3*i+1])) != r2[i] )
            data = data ^ 0x4;
        if ( ((true != r[3*i] ) && (true != r[3*i+1])) != r2[i] )
            data = data ^ 0x8;
        return data;
    }
    bool evaluate(uint8_t tmp, block * MAC, bool * r, int i, int I) {
        block hin = sigma(MAC[3*i]) ^ makeBlock(0, 4*i + r[3*i]);
        block hin2 = sigma(MAC[3*i+1]) ^ makeBlock(0, 4*i + 2 + r[3*i+1]);
        block bH = prps[I].H(hin) ^ prps[I].H(hin2);
        uint8_t res = getLSB(bH);
        tmp >>= (r[3*i+1]*2+r[3*i]);
        return (tmp&0x1) != (res&0x1);
    }

    void check_MAC_phi(const NVec<block>& MAC, const NVec<block>& KEY, block * phi, bool * r, int length) {
        block * tmp = new block[length];
        block *tD = new block[length];
        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if (i < j) {
            if(party == i) {
                io->send_channel(j).send_data(phi, length*sizeof(block));
                io->send_channel(j).send_data(&KEY.at(j, 0), sizeof(block)*length);
                io->flush(j);
            } else if(party == j) {
                io->recv_channel(i).recv_data(tD, length*sizeof(block));
                io->recv_channel(i).recv_data(tmp, sizeof(block)*length);
                for(int k = 0; k < length; ++k) {
                    if(r[k])tmp[k] = tmp[k] ^ tD[k];
                }
                if(!cmpBlock(&MAC.at(i, 0), tmp, length))
                    error("check_MAC_phi failed!");
            }
        }
        delete[] tmp;
        delete[] tD;
        if(party == 1)
            cerr<<"check_MAC_phi pass!\n"<<flush;
    }


    void check_zero(block * b, int l) {
        if(party == 1) {
            block * tmp1 = new block[l];
            block * tmp2 = new block[l];
            memcpy(tmp1, b, l*sizeof(block));
            for(int i = 2; i <= nP; ++i) {
                io->recv_channel(i).recv_data(tmp2, l*sizeof(block));
                xorBlocks_arr(tmp1, tmp1, tmp2, l);
            }
            block z = zero_block;
            for(int i = 0; i < l; ++i)
                if(!cmpBlock(&z, &tmp1[i], 1))
                    error("check sum zero failed!");
            cerr<<"check zero sum pass!\n"<<flush;
            delete[] tmp1;
            delete[] tmp2;
        } else {
            io->send_channel(1).send_data(b, l*sizeof(block));
            io->flush(1);
        }
    }
    void HnID(CRH* crh, block*out, block* in, uint64_t id, int length, block * scratch = nullptr) {
        bool del = false;
        if(scratch == nullptr) {
            del = true;
            scratch = new block[length];
        }
        for(int i = 0; i < length; ++i){
            out[i] = scratch[i] = sigma(in[i]) ^ makeBlock(0, id);
            ++id;
        }
        crh->permute_block(scratch, length);
        xorBlocks_arr(out, scratch, out, length);
        if(del) {
            delete[] scratch;
            scratch = nullptr;
        }
    }
};
#endif// FPRE_H
