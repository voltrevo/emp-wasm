#ifndef CMPC_H
#define CMPC_H
#include "fpremp.h"
#include "abitmp.h"
#include "netmp.h"
#include "vec.h"
#include "nvec.h"
#include "flexible_input_output.h"
#include <emp-tool/emp-tool.h>
using namespace emp;

template <int nP_deprecated>
class CMPC { public:
    const static int SSP = 5;//5*8 in fact...
    const block MASK = makeBlock(0x0ULL, 0xFFFFFULL);
    FpreMP<nP_deprecated>* fpre = nullptr;

    int nP;

    NVec<block> mac; // dim: parties, wires
    NVec<block> key; // dim: parties, wires
    Vec<bool> value; // dim: wires

    NVec<block> preprocess_mac; // dim: parties, total_pre
    NVec<block> preprocess_key; // dim: parties, total_pre
    Vec<bool> preprocess_value; // dim: total_pre

    NVec<block> sigma_mac; // dim: parties, num_ands
    NVec<block> sigma_key; // dim: parties, num_ands
    Vec<bool> sigma_value; // dim: num_ands

    NVec<block> ANDS_mac; // dim: parties, num_ands*3
    NVec<block> ANDS_key; // dim: parties, num_ands*3
    Vec<bool> ANDS_value; // dim: num_ands*3

    Vec<block> labels; // dim: wires
    BristolFormat * cf;
    NetIOMP * io;
    int num_ands = 0, num_in;
    int party, total_pre, ssp;
    block Delta;

    NVec<block> GTM; // dim: num_ands, 4, parties
    NVec<block> GTK; // dim: num_ands, 4, parties
    NVec<bool> GTv; // dim: num_ands, 4
    NVec<block> GT; // dim: num_ands, parties, 4, parties
    NVec<block> eval_labels; // dim: parties, wires
    PRP prp;

    CMPC(
        int nP,
        NetIOMP * io[2],
        int party,
        BristolFormat * cf,
        bool * _delta = nullptr,
        int ssp = 40
    ) {
        this->nP = nP;
        this->party = party;
        this->io = io[0];
        this->cf = cf;
        this->ssp = ssp;

        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == AND_GATE)
                ++num_ands;
        }
        num_in = cf->n1+cf->n2;
        total_pre = num_in + num_ands + 3*ssp;
        fpre = new FpreMP<nP_deprecated>(nP, io, party, _delta, ssp);
        Delta = fpre->Delta;

        if(party == 1) {
            GTM.resize(num_ands, 4, nP+1);
            GTK.resize(num_ands, 4, nP+1);
            GTv.resize(num_ands, 4);
            GT.resize(num_ands, nP+1, 4, nP+1);
        }

        labels.resize(cf->num_wire);
        key.resize(nP+1, cf->num_wire);
        mac.resize(nP+1, cf->num_wire);
        ANDS_key.resize(nP+1, num_ands*3);
        ANDS_mac.resize(nP+1, num_ands*3);
        preprocess_mac.resize(nP+1, total_pre);
        preprocess_key.resize(nP+1, total_pre);
        sigma_mac.resize(nP+1, num_ands);
        sigma_key.resize(nP+1, num_ands);
        eval_labels.resize(nP+1, cf->num_wire);

        value.resize(cf->num_wire);
        ANDS_value.resize(num_ands*3);
        preprocess_value.resize(total_pre);
        sigma_value.resize(num_ands);
    }
    ~CMPC() {
        delete fpre;
    }
    PRG prg;

    void function_independent() {
        if(party != 1)
            prg.random_block(&labels[0], cf->num_wire);

        fpre->compute(ANDS_mac, ANDS_key, &ANDS_value[0], num_ands);

        prg.random_bool(&preprocess_value[0], total_pre);
        fpre->abit->compute(preprocess_mac, preprocess_key, &preprocess_value[0], total_pre);
        fpre->abit->check(preprocess_mac, preprocess_key, &preprocess_value[0], total_pre);

        for(int i = 1; i <= nP; ++i) {
            memcpy(&key.at(i, 0), &preprocess_key.at(i, 0), num_in * sizeof(block));
            memcpy(&mac.at(i, 0), &preprocess_mac.at(i, 0), num_in * sizeof(block));
        }
        memcpy(&value[0], &preprocess_value[0], num_in * sizeof(bool));
#ifdef __debug
        check_MAC(nP, io, ANDS_mac, ANDS_key, &ANDS_value[0], Delta, num_ands*3, party);
        check_correctness(nP, io, &ANDS_value[0], num_ands, party);
#endif
//        ret.get();
    }

    void function_dependent() {
        int ands = num_in;
        NVec<bool> x(nP+1, num_ands);
        NVec<bool> y(nP+1, num_ands);

        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == AND_GATE) {
                for(int j = 1; j <= nP; ++j) {
                    key.at(j, cf->gates[4*i+2]) = preprocess_key.at(j, ands);
                    mac.at(j, cf->gates[4*i+2]) = preprocess_mac.at(j, ands);
                }
                value[cf->gates[4*i+2]] = preprocess_value[ands];
                ++ands;
            }
        }

        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == XOR_GATE) {
                for(int j = 1; j <= nP; ++j) {
                    key.at(j, cf->gates[4*i+2]) = key.at(j, cf->gates[4*i]) ^ key.at(j, cf->gates[4*i+1]);
                    mac.at(j, cf->gates[4*i+2]) = mac.at(j, cf->gates[4*i]) ^ mac.at(j, cf->gates[4*i+1]);
                }
                value[cf->gates[4*i+2]] = value[cf->gates[4*i]] != value[cf->gates[4*i+1]];
                if(party != 1)
                    labels[cf->gates[4*i+2]] = labels[cf->gates[4*i]] ^ labels[cf->gates[4*i+1]];
            } else if (cf->gates[4*i+3] == NOT_GATE) {
                for(int j = 1; j <= nP; ++j) {
                    key.at(j, cf->gates[4*i+2]) = key.at(j, cf->gates[4*i]);
                    mac.at(j, cf->gates[4*i+2]) = mac.at(j, cf->gates[4*i]);
                }
                value[cf->gates[4*i+2]] = value[cf->gates[4*i]];
                if(party != 1)
                    labels[cf->gates[4*i+2]] = labels[cf->gates[4*i]] ^ Delta;
            }
        }

#ifdef __debug
        check_MAC(nP, io, mac, key, &value[0], Delta, cf->num_wire, party);
#endif

        ands = 0;
        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == AND_GATE) {
                x.at(party, ands) = value[cf->gates[4*i]] != ANDS_value[3*ands];
                y.at(party, ands) = value[cf->gates[4*i+1]] != ANDS_value[3*ands+1];
                ands++;
            }
        }

        for(int i = 1; i <= nP; ++i) for(int j = 1; j <= nP; ++j) if( (i < j) and (i == party or j == party) ) {
            int party2 = i + j - party;

            io->send_data(party2, &x.at(party, 0), num_ands);
            io->send_data(party2, &y.at(party, 0), num_ands);
            io->flush(party2);

            io->recv_data(party2, &x.at(party2, 0), num_ands);
            io->recv_data(party2, &y.at(party2, 0), num_ands);
        }
        for(int i = 2; i <= nP; ++i) for(int j = 0; j < num_ands; ++j) {
            x.at(1, j) = x.at(1, j) != x.at(i, j);
            y.at(1, j) = y.at(1, j) != y.at(i, j);
        }

        ands = 0;
        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == AND_GATE) {
                for(int j = 1; j <= nP; ++j) {
                    sigma_mac.at(j, ands) = ANDS_mac.at(j, 3*ands+2);
                    sigma_key.at(j, ands) = ANDS_key.at(j, 3*ands+2);
                }
                sigma_value[ands] = ANDS_value[3*ands+2];

                if(x.at(1, ands)) {
                    for(int j = 1; j <= nP; ++j) {
                        sigma_mac.at(j, ands) = sigma_mac.at(j, ands) ^ ANDS_mac.at(j, 3*ands+1);
                        sigma_key.at(j, ands) = sigma_key.at(j, ands) ^ ANDS_key.at(j, 3*ands+1);
                    }
                    sigma_value[ands] = sigma_value[ands] != ANDS_value[3*ands+1];
                }
                if(y.at(1, ands)) {
                    for(int j = 1; j <= nP; ++j) {
                        sigma_mac.at(j, ands) = sigma_mac.at(j, ands) ^ ANDS_mac.at(j, 3*ands);
                        sigma_key.at(j, ands) = sigma_key.at(j, ands) ^ ANDS_key.at(j, 3*ands);
                    }
                    sigma_value[ands] = sigma_value[ands] != ANDS_value[3*ands];
                }
                if(x.at(1, ands) and y.at(1, ands)) {
                    if(party != 1)
                        sigma_key.at(1, ands) = sigma_key.at(1, ands) ^ Delta;
                    else
                        sigma_value[ands] = not sigma_value[ands];
                }
                ands++;
            }
        }//sigma_[] stores the and of input wires to each AND gates
#ifdef __debug_
        check_MAC(nP, io, sigma_mac, sigma_key, sigma_value, Delta, num_ands, party);
        ands = 0;
        for(int i = 0; i < cf->num_gate; ++i) {
            if (cf->gates[4*i+3] == AND_GATE) {
                bool tmp[] = { value[cf->gates[4*i]], value[cf->gates[4*i+1]], sigma_value[ands]};
                check_correctness(io, tmp, 1, party);
                ands++;
            }
        }
#endif

        ands = 0;
        NVec<block> H(4, nP+1);
        NVec<block> K(4, nP+1);
        NVec<block> M(4, nP+1);
        bool r[4];
        if(party != 1) {
            for(int i = 0; i < cf->num_gate; ++i) if(cf->gates[4*i+3] == AND_GATE) {
                r[0] = sigma_value[ands] != value[cf->gates[4*i+2]];
                r[1] = r[0] != value[cf->gates[4*i]];
                r[2] = r[0] != value[cf->gates[4*i+1]];
                r[3] = r[1] != value[cf->gates[4*i+1]];

                for(int j = 1; j <= nP; ++j) {
                    M.at(0, j) = sigma_mac.at(j, ands) ^ mac.at(j, cf->gates[4*i+2]);
                    M.at(1, j) = M.at(0, j) ^ mac.at(j, cf->gates[4*i]);
                    M.at(2, j) = M.at(0, j) ^ mac.at(j, cf->gates[4*i+1]);
                    M.at(3, j) = M.at(1, j) ^ mac.at(j, cf->gates[4*i+1]);

                    K.at(0, j) = sigma_key.at(j, ands) ^ key.at(j, cf->gates[4*i+2]);
                    K.at(1, j) = K.at(0, j) ^ key.at(j, cf->gates[4*i]);
                    K.at(2, j) = K.at(0, j) ^ key.at(j, cf->gates[4*i+1]);
                    K.at(3, j) = K.at(1, j) ^ key.at(j, cf->gates[4*i+1]);
                }
                K.at(3, 1) = K.at(3, 1) ^ Delta;

                Hash(H, labels[cf->gates[4*i]], labels[cf->gates[4*i+1]], ands);
                for(int j = 0; j < 4; ++j) {
                    for(int k = 1; k <= nP; ++k) if(k != party) {
                        H.at(j, k) = H.at(j, k) ^ M.at(j, k);
                        H.at(j, party) = H.at(j, party) ^ K.at(j, k);
                    }
                    H.at(j, party) = H.at(j, party) ^ labels[cf->gates[4*i+2]];
                    if(r[j])
                        H.at(j, party) = H.at(j, party) ^ Delta;
                }
                for(int j = 0; j < 4; ++j)
                    io->send_data(1, &H.at(j, 1), sizeof(block)*(nP));
                ++ands;
            }
            io->flush(1);
        } else {
            for(int i = 2; i <= nP; ++i) {
                int party2 = i;
                for(int i = 0; i < num_ands; ++i)
                    for(int j = 0; j < 4; ++j)
                        io->recv_data(party2, &GT.at(i, party2, j, 1), sizeof(block)*(nP));
            }
            for(int i = 0; i < cf->num_gate; ++i) if(cf->gates[4*i+3] == AND_GATE) {
                r[0] = sigma_value[ands] != value[cf->gates[4*i+2]];
                r[1] = r[0] != value[cf->gates[4*i]];
                r[2] = r[0] != value[cf->gates[4*i+1]];
                r[3] = r[1] != value[cf->gates[4*i+1]];
                r[3] = r[3] != true;

                for(int j = 1; j <= nP; ++j) {
                    M.at(0, j) = sigma_mac.at(j, ands) ^ mac.at(j, cf->gates[4*i+2]);
                    M.at(1, j) = M.at(0, j) ^ mac.at(j, cf->gates[4*i]);
                    M.at(2, j) = M.at(0, j) ^ mac.at(j, cf->gates[4*i+1]);
                    M.at(3, j) = M.at(1, j) ^ mac.at(j, cf->gates[4*i+1]);

                    K.at(0, j) = sigma_key.at(j, ands) ^ key.at(j, cf->gates[4*i+2]);
                    K.at(1, j) = K.at(0, j) ^ key.at(j, cf->gates[4*i]);
                    K.at(2, j) = K.at(0, j) ^ key.at(j, cf->gates[4*i+1]);
                    K.at(3, j) = K.at(1, j) ^ key.at(j, cf->gates[4*i+1]);
                }
                memcpy(&GTK.at(ands, 0, 0), &K.at(0, 0), sizeof(block)*4*(nP+1));
                memcpy(&GTM.at(ands, 0, 0), &M.at(0, 0), sizeof(block)*4*(nP+1));
                memcpy(&GTv.at(ands, 0), r, sizeof(bool)*4);
                ++ands;
            }
        }
    }
    void Hash(NVec<block>& H, const block & a, const block & b, uint64_t idx) {
        block T[4];
        T[0] = sigma(a);
        T[1] = sigma(a ^ Delta);
        T[2] = sigma(sigma(b));
        T[3] = sigma(sigma(b ^ Delta));

        H.at(0, 0) = T[0] ^ T[2];
        H.at(1, 0) = T[0] ^ T[3];
        H.at(2, 0) = T[1] ^ T[2];
        H.at(3, 0) = T[1] ^ T[3];
        for(int j = 0; j < 4; ++j) for(int i = 1; i <= nP; ++i) {
            H.at(j, i) = H.at(j, 0) ^ makeBlock(4*idx+j, i);
        }
        for(int j = 0; j < 4; ++j) {
            prp.permute_block(&H.at(j, 1), nP);
        }
    }

    void Hash(block* H, const block &a, const block& b, uint64_t idx, uint64_t row) {
        H[0] = sigma(a) ^ sigma(sigma(b));
        for(int i = 1; i <= nP; ++i) {
            H[i] = H[0] ^ makeBlock(4*idx+row, i);
        }
        prp.permute_block(H+1, nP);
    }

    string tostring(bool a) {
        if(a) return "T";
        else return "F";
    }

    void online (FlexIn* input, FlexOut* output) {
        bool * mask_input = new bool[cf->num_wire];
        input->associate_cmpc(&value[0], mac, key, io, Delta);
        input->input(mask_input);

        if(party!= 1) {
            for(int i = 0; i < num_in; ++i) {
                block tmp = labels[i];
                if(mask_input[i]) tmp = tmp ^ Delta;
                io->send_data(1, &tmp, sizeof(block));
            }
            io->flush(1);
        } else {
            for(int i = 2; i <= nP; ++i) {
                int party2 = i;
                io->recv_data(party2, &eval_labels.at(party2, 0), num_in*sizeof(block));
            }

            int ands = 0;
            for(int i = 0; i < cf->num_gate; ++i) {
                if (cf->gates[4*i+3] == XOR_GATE) {
                    for(int j = 2; j<= nP; ++j)
                        eval_labels.at(j, cf->gates[4*i+2]) = eval_labels.at(j, cf->gates[4*i]) ^ eval_labels.at(j, cf->gates[4*i+1]);
                    mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i]] != mask_input[cf->gates[4*i+1]];
                } else if (cf->gates[4*i+3] == AND_GATE) {
                    int index = 2*mask_input[cf->gates[4*i]] + mask_input[cf->gates[4*i+1]];
                    block H[nP+1];
                    for(int j = 2; j <= nP; ++j)
                        eval_labels.at(j, cf->gates[4*i+2]) = GTM.at(ands, index, j);
                    mask_input[cf->gates[4*i+2]] = GTv.at(ands, index);
                    for(int j = 2; j <= nP; ++j) {
                        Hash(H, eval_labels.at(j, cf->gates[4*i]), eval_labels.at(j, cf->gates[4*i+1]), ands, index);
                        xorBlocks_arr(H, H, &GT.at(ands, j, index, 0), nP+1);
                        for(int k = 2; k <= nP; ++k)
                            eval_labels.at(k, cf->gates[4*i+2]) = H[k] ^ eval_labels.at(k, cf->gates[4*i+2]);

                        block t0 = GTK.at(ands, index, j) ^ Delta;

                        if(cmpBlock(&H[1], &GTK.at(ands, index, j), 1))
                            mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != false;
                        else if(cmpBlock(&H[1], &t0, 1))
                            mask_input[cf->gates[4*i+2]] = mask_input[cf->gates[4*i+2]] != true;
                        else     {cout <<ands <<"no match GT!"<<endl<<flush;
                        }
                    }
                    ands++;
                } else {
                    mask_input[cf->gates[4*i+2]] = not mask_input[cf->gates[4*i]];
                    for(int j = 2; j <= nP; ++j)
                        eval_labels.at(j, cf->gates[4*i+2]) = eval_labels.at(j, cf->gates[4*i]);
                }
            }
        }

        output->associate_cmpc(&value[0], mac, key, eval_labels, labels, io, Delta);
        output->output(mask_input, cf->num_wire - cf->n3);

        delete[] mask_input;
    }
};
#endif// CMPC_H
