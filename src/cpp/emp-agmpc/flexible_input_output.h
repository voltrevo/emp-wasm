#ifndef EMP_AGMPC_FLEXIBLE_INPUT_OUTPUT_H
#define EMP_AGMPC_FLEXIBLE_INPUT_OUTPUT_H

#include "vec.h"

using namespace std;

struct AuthBitShare
{
    int nP;
    bool bit_share;
    Vec<block> key;
    Vec<block> mac;

    AuthBitShare(int nP) {
        this->nP = nP;
        this->bit_share = false;
        key.resize(nP + 1);
        mac.resize(nP + 1);
    }

    AuthBitShare(const AuthBitShare& abit) {
        this->nP = abit.nP;
        this->bit_share = abit.bit_share;
        key = abit.key;
        mac = abit.mac;
    }

    AuthBitShare& operator=(const AuthBitShare& abit) {
        this->nP = abit.nP;
        this->bit_share = abit.bit_share;
        key = abit.key;
        mac = abit.mac;
        return *this;
    }
};

struct BitWithMac
{
    bool bit_share;
    block mac;
};

class FlexIn
{
public:

    int nP;

    int len{};
    int party{};

    bool cmpc_associated = false;
    bool *value;
    NVec<block>* key;
    NVec<block>* mac;
    NetIOMP* io;
    block Delta;

    vector<int> party_assignment;
    // -2 represents an un-authenticated share (e.g., for random tape),
    // -1 represents an authenticated share,
    //  0 represents public input/output,

    vector<bool> plaintext_assignment; // if `party` provides the value for this bit, the plaintext value is here
    vector<AuthBitShare> authenticated_share_assignment; // if this bit is from authenticated shares, the authenticated share is stored here

    FlexIn(int nP, int len, int party) {
        this->nP = nP;
        this->len = len;
        this->party = party;

        party_assignment.resize(len, 0);
        plaintext_assignment.resize(len, false);
        authenticated_share_assignment.resize(len, AuthBitShare(nP));
    }

    ~FlexIn() {
        party_assignment.clear();
        plaintext_assignment.clear();
        authenticated_share_assignment.clear();
    }

    void associate_cmpc(bool *associated_value, NVec<block>& associated_mac, NVec<block>& associated_key,  NetIOMP *associated_io, block associated_Delta) {
        this->cmpc_associated = true;
        this->value = associated_value;
        this->mac = &associated_mac;
        this->key = &associated_key;
        this->io = associated_io;
        this->Delta = associated_Delta;
    }

    void assign_party(int pos, int which_party) {
        party_assignment[pos] = which_party;
    }

    void assign_plaintext_bit(int pos, bool cur_bit) {
        assert(party_assignment[pos] == party || party_assignment[pos] == -2  || party_assignment[pos] == 0);
        plaintext_assignment[pos] = cur_bit;
    }

    void assign_authenticated_bitshare(int pos, AuthBitShare *abit) {
        assert(party_assignment[pos] == -1);
        authenticated_share_assignment[pos] = *abit;
    }

    void input(bool *masked_input_ret) {
        assert(cmpc_associated);

        /* assemble an array of the input masks, their macs, and their keys */
        /*
         * Then,
         *         for a plaintext bit, the input mask, as well as its MAC, is sent to the input party, who uses the KEY for verification;
         *         for an un-authenticated bit, the input mask XOR with the input share is broadcast;
         *        for an authenticated bit share, they are used to masked the previously data (and then checking its opening)
         */
        vector<AuthBitShare> input_mask;
        for(int i = 0; i < len; i++) {
            AuthBitShare abit(nP);
            abit.bit_share = value[i];
            for(int j = 1; j <= nP; j++) {
                if(j != party) {
                    abit.key[j] = key->at(j, i);
                    abit.mac[j] = mac->at(j, i);
                }
            }
            input_mask.emplace_back(abit);
        }

        /*
         * first of all, handle the case party_assignment[] > 0
         */

        /* prepare the bit shares to open for the corresponding party */
        vector<vector<BitWithMac>> open_bit_shares_for_plaintext_input_send;
        open_bit_shares_for_plaintext_input_send.resize(nP + 1);

        for(int j = 1; j <= nP; j++) {
            if(j != party) {
                for(int i = 0; i < len; i++) {
                    BitWithMac mbit{};
                    if(party_assignment[i] == j) {
                        mbit.bit_share = input_mask[i].bit_share;
                        mbit.mac = input_mask[i].mac[j];
                    }
                    open_bit_shares_for_plaintext_input_send[j].push_back(mbit);
                }
            }
        }

        vector<vector<BitWithMac>> open_bit_shares_for_plaintext_input_recv;
        open_bit_shares_for_plaintext_input_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            open_bit_shares_for_plaintext_input_recv[j].resize(len);
        }

        /*
         * exchange the opening of the input mask
         */
        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, open_bit_shares_for_plaintext_input_send[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                    io->recv_data(party2, open_bit_shares_for_plaintext_input_recv[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                }
            }
        }

        /*
         * verify the input mask
         */
        vector<bool> res_check;
        for (int j = 1; j <= nP; ++j) {
            if(j != party) {
                bool check = false;
                for (int i = 0; i < len; i++) {
                    if (party_assignment[i] == party) {
                        block supposed_mac = Delta & select_mask[open_bit_shares_for_plaintext_input_recv[j][i].bit_share? 1 : 0];
                        supposed_mac ^= input_mask[i].key[j];

                        block provided_mac = open_bit_shares_for_plaintext_input_recv[j][i].mac;

                        if(!cmpBlock(&supposed_mac, &provided_mac, 1)) {
                            check = true;
                            break;
                        }
                    }
                }
                res_check.push_back(check);
            }
        }
        if(checkCheat(res_check)) error("cheat in FlexIn's plaintext input mask!");

        /*
         * broadcast the masked input
         */
        vector<char> masked_input_sent;    // use char instead of bool because bools seem to fail for "data()"
        vector<vector<char>> masked_input_recv;
        masked_input_sent.resize(len);
        masked_input_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            masked_input_recv[j].resize(len);
        }

        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == party) {
                masked_input_sent[i] = plaintext_assignment[i] ^ input_mask[i].bit_share;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        masked_input_sent[i] = masked_input_sent[i] ^ open_bit_shares_for_plaintext_input_recv[j][i].bit_share;
                    }
                }
            }
        }

        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, masked_input_sent.data(), sizeof(char) * len);
                    io->flush(party2);
                    io->recv_data(party2, masked_input_recv[party2].data(), sizeof(char) * len);
                    io->flush(party2);
                }
            }
        }

        vector<bool> masked_input;
        masked_input.resize(len);
        for(int i = 0; i < len; i++) {
            if(party_assignment[i] > 0) {
                int this_party = party_assignment[i];
                if(this_party == party) {
                    masked_input[i] = masked_input_sent[i];
                } else {
                    masked_input[i] = masked_input_recv[this_party][i];
                }
            }
        }

        /*
         * secondly, handle the case party_assignment[] == -1
         */

        /*
         * Compute the authenticated bit share to the new circuit
         * by XOR-ing with the input mask
         */
        vector<AuthBitShare> authenticated_bitshares_new_circuit;
        for(int i = 0; i < len; i++) {
            AuthBitShare new_entry(nP);
            if(party_assignment[i] == -1) {
                new_entry.bit_share = authenticated_share_assignment[i].bit_share ^ input_mask[i].bit_share;
                for (int j = 1; j <= nP; j++) {
                    new_entry.key[j] = authenticated_share_assignment[i].key[j] ^ input_mask[i].key[j];
                    new_entry.mac[j] = authenticated_share_assignment[i].mac[j] ^ input_mask[i].mac[j];
                }
            }
            authenticated_bitshares_new_circuit.emplace_back(new_entry);
        }

        //print_block(Delta);

        /*
        cout << "Debug the authenticated input" << endl;
        for(int i = 0; i < 10; i++){
            if(party_assignment[i] == -1) {
                cout << "index: " << i << ", value: " << authenticated_share_assignment[i].bit_share << endl;
                cout << "mac: " << endl;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        //cout << j << ": ";
                        print_block(authenticated_share_assignment[i].mac[j]);
                    }
                }
                cout << "key: " << endl;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        //cout << j << ": ";
                        print_block(authenticated_share_assignment[i].key[j]);
                        print_block(authenticated_share_assignment[i].key[j] ^ Delta);
                    }
                }
                cout << "=============" << endl;
            }
        }
         */

        /*
         * Opening the authenticated shares
         */
        vector<vector<BitWithMac>> open_bit_shares_for_authenticated_bits_send;
        open_bit_shares_for_authenticated_bits_send.resize(nP + 1);

        for(int j = 1; j <= nP; j++) {
            if(j != party) {
                for(int i = 0; i < len; i++) {
                    BitWithMac mbit{};
                    if(party_assignment[i] == -1) {
                        mbit.bit_share = authenticated_bitshares_new_circuit[i].bit_share;
                        mbit.mac = authenticated_bitshares_new_circuit[i].mac[j];
                    }
                    open_bit_shares_for_authenticated_bits_send[j].push_back(mbit);
                }
            }
        }

        vector<vector<BitWithMac>> open_bit_shares_for_authenticated_bits_recv;
        open_bit_shares_for_authenticated_bits_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            open_bit_shares_for_authenticated_bits_recv[j].resize(len);
        }

        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, open_bit_shares_for_authenticated_bits_send[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                    io->recv_data(party2, open_bit_shares_for_authenticated_bits_recv[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                }
            }
        }

        /*
         * verify the input mask shares
         */
        for (int j = 1; j <= nP; ++j) {
            if(j != party) {
                bool check = false;
                for (int i = 0; i < len; i++) {
                    if (party_assignment[i] == -1) {
                        block supposed_mac = Delta & select_mask[open_bit_shares_for_authenticated_bits_recv[j][i].bit_share? 1 : 0];
                        supposed_mac ^= authenticated_bitshares_new_circuit[i].key[j];

                        block provided_mac = open_bit_shares_for_authenticated_bits_recv[j][i].mac;

                        if(!cmpBlock(&supposed_mac, &provided_mac, 1)) {
                            check = true;
                            break;
                        }
                    }
                }
                res_check.push_back(check);
            }
        }
        if(checkCheat(res_check)) error("cheat in FlexIn's authenticated share input mask!");

        /*
         * Reconstruct the authenticated shares
         */
        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == -1) {
                masked_input[i] = authenticated_bitshares_new_circuit[i].bit_share;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        masked_input[i] = masked_input[i] ^ open_bit_shares_for_authenticated_bits_recv[j][i].bit_share;
                    }
                }
            }
        }

        /*
         * thirdly, handle the case party_assignment[] = -2
         */

        /*
         * Collect the masked input shares for un-authenticated bits
         */
        vector<char> open_bit_shares_for_unauthenticated_bits_send;
        open_bit_shares_for_unauthenticated_bits_send.resize(len);

        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == -2) {
                open_bit_shares_for_unauthenticated_bits_send[i] = (plaintext_assignment[i] ^ input_mask[i].bit_share)? 1 : 0;
            }
        }

        vector<vector<char>> open_bit_shares_for_unauthenticated_bits_recv;
        open_bit_shares_for_unauthenticated_bits_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            open_bit_shares_for_unauthenticated_bits_recv[j].resize(len);
        }

        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, open_bit_shares_for_unauthenticated_bits_send.data(), sizeof(char) * len);
                    io->flush(party2);
                    io->recv_data(party2, open_bit_shares_for_unauthenticated_bits_recv[party2].data(), sizeof(char) * len);
                    io->flush(party2);
                }
            }
        }

        /*
         * update the array of masked_input accordingly
         */
        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == -2) {
                masked_input[i] = open_bit_shares_for_unauthenticated_bits_send[i];
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        masked_input[i] = masked_input[i] ^ (open_bit_shares_for_unauthenticated_bits_recv[j][i] == 1);
                    }
                }
            }
        }

        /*
         * lastly, handle the case party_assignment[] = 0
         */

        /*
         * broadcast the input mask and its MAC
         */
        vector<vector<BitWithMac>> open_bit_shares_for_public_input_send;
        open_bit_shares_for_public_input_send.resize(nP + 1);

        for(int j = 1; j <= nP; j++) {
            if(j != party) {
                for(int i = 0; i < len; i++) {
                    BitWithMac mbit{};
                    if(party_assignment[i] == 0) {
                        mbit.bit_share = input_mask[i].bit_share;
                        mbit.mac = input_mask[i].mac[j];
                    }
                    open_bit_shares_for_public_input_send[j].push_back(mbit);
                }
            }
        }

        vector<vector<BitWithMac>> open_bit_shares_for_public_input_recv;
        open_bit_shares_for_public_input_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            open_bit_shares_for_public_input_recv[j].resize(len);
        }

        /*
         * exchange the opening of the input mask
         */
        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, open_bit_shares_for_public_input_send[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                    io->recv_data(party2, open_bit_shares_for_public_input_recv[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                }
            }
        }

        /*
         * verify the input mask
         */
        for (int j = 1; j <= nP; ++j) {
            if(j != party) {
                bool check = false;
                for (int i = 0; i < len; i++) {
                    if (party_assignment[i] == 0) {
                        block supposed_mac = Delta & select_mask[open_bit_shares_for_public_input_recv[j][i].bit_share? 1 : 0];
                        supposed_mac ^= input_mask[i].key[j];

                        block provided_mac = open_bit_shares_for_public_input_recv[j][i].mac;

                        if(!cmpBlock(&supposed_mac, &provided_mac, 1)) {
                            check = true;
                            break;
                        }
                    }
                }
                res_check.push_back(check);
            }
        }
        if(checkCheat(res_check)) error("cheat in FlexIn's public input mask!");

        /*
         * update the masked input
         */
        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == 0) {
                masked_input[i] = plaintext_assignment[i] ^ input_mask[i].bit_share;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        masked_input[i] = masked_input[i] ^ open_bit_shares_for_public_input_recv[j][i].bit_share;
                    }
                }
            }
        }

        /*
         cout << "masked_input" << endl;
        for(int i = 0; i < len; i++) {
            cout << masked_input[i] << " ";
        }
        cout << endl;
         */

        for(int i = 0; i < len; i++) {
            masked_input_ret[i] = masked_input[i];
        }
    }

    int get_length() {
        return len;
    }
};

class FlexOut
{
public:

    int nP;

    int len{};
    int party{};

    bool cmpc_associated = false;
    bool *value;
    NVec<block>* key;
    NVec<block>* mac;
    NVec<block>* eval_labels;
    NetIOMP * io;
    block Delta;
    block *labels;

    vector<int> party_assignment;
    // -1 represents an authenticated share,
    //  0 represents public output

    vector<bool> plaintext_results; // if `party` provides the value for this bit, the plaintext value is here
    vector<AuthBitShare> authenticated_share_results; // if this bit is from authenticated shares, the authenticated share is stored here

    FlexOut(int nP, int len, int party) {
        this->nP = nP;
        this->len = len;
        this->party = party;

        AuthBitShare empty_abit(nP);

        party_assignment.resize(len, 0);
        plaintext_results.resize(len, false);
        authenticated_share_results.resize(len, empty_abit);
    }

    ~FlexOut() {
        party_assignment.clear();
        plaintext_results.clear();
        authenticated_share_results.clear();
    }

    void associate_cmpc(bool *associated_value, NVec<block>& associated_mac, NVec<block>& associated_key, NVec<block>& associated_eval_labels, Vec<block>& associated_labels, NetIOMP *associated_io, block associated_Delta) {
        this->cmpc_associated = true;
        this->value = associated_value;
        this->labels = &associated_labels.at(0);
        this->mac = &associated_mac;
        this->key = &associated_key;
        if (party == ALICE){
            this->eval_labels = &associated_eval_labels;
        }
        this->io = associated_io;
        this->Delta = associated_Delta;
    }

    void assign_party(int pos, int which_party) {
        party_assignment[pos] = which_party;
    }

    bool get_plaintext_bit(int pos) {
        assert(party_assignment[pos] == party || party_assignment[pos] == 0);
        return plaintext_results[pos];
    }

    int get_length() {
        return len;
    }

    void output(bool *masked_input_ret, int output_shift) {
        assert(cmpc_associated);

        /*
         * Party 1 sends the labels of all the output wires out.
         */
        vector<block> output_wire_label_recv;
        output_wire_label_recv.resize(len);

        if(party == ALICE) {
            vector<vector<block>> output_wire_label_send;
            output_wire_label_send.resize(nP + 1);

            for (int j = 2; j <= nP; j++) {
                output_wire_label_send[j].resize(len);
                for(int i = 0; i < len; i++) {
                    output_wire_label_send[j][i] = eval_labels->at(j, output_shift + i);
                }
            }

            for(int j = 2; j <= nP; j++) {
                io->send_data(j, output_wire_label_send[j].data(), sizeof(block) * len);
                io->flush(j);
            }
        }else {
            io->recv_data(ALICE, output_wire_label_recv.data(), sizeof(block) * len);
            io->flush(ALICE);
        }

        /*
         * Each party extracts x ^ r of each output wire
         */
        vector<bool> masked_output;
        masked_output.resize(len);

        if(party == ALICE) {
            for(int i = 0; i < len; i++) {
                masked_output[i] = masked_input_ret[output_shift + i];
            }
        } else {
            for(int i = 0; i < len; i++) {
                block cur_label = output_wire_label_recv[i];
                block zero_label = labels[i + output_shift];
                block one_label = zero_label ^ Delta;

                if(cmpBlock(&cur_label, &zero_label, 1)) {
                    masked_output[i] = false;
                } else if(cmpBlock(&cur_label, &one_label, 1)) {
                    masked_output[i] = true;
                } else {
                    error("Output label mismatched.\n");
                }
            }
        }

        /*
         * Decide the broadcasting of the shares of r, as well as their MAC
         */
        vector<vector<BitWithMac>> output_mask_send;
        output_mask_send.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            BitWithMac empty_mbit{};
            output_mask_send[j].resize(len, empty_mbit);
        }

        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == -1) {
                // do nothing, just update the share (for the first party) later
            } else if(party_assignment[i] == 0) {
                // public output, all parties receive the mbit
                for(int j = 1; j <= nP; j++){
                    output_mask_send[j][i].bit_share = value[output_shift + i];
                    output_mask_send[j][i].mac = mac->at(j, output_shift + i);
                }
            } else {
                // only one party is supposed to receive the mbit
                int cur_party = party_assignment[i];
                output_mask_send[cur_party][i].bit_share = value[output_shift + i];
                output_mask_send[cur_party][i].mac = mac->at(cur_party, output_shift + i);
            }
        }

        /*
         * Exchange the output mask
         */
        vector<vector<BitWithMac>> output_mask_recv;
        output_mask_recv.resize(nP + 1);
        for(int j = 1; j <= nP; j++) {
            output_mask_recv[j].resize(len);
        }

        for (int i = 1; i <= nP; ++i) {
            for (int j = 1; j <= nP; ++j) {
                if ((i < j) and (i == party or j == party)) {
                    int party2 = i + j - party;

                    io->send_data(party2, output_mask_send[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                    io->recv_data(party2, output_mask_recv[party2].data(), sizeof(BitWithMac) * len);
                    io->flush(party2);
                }
            }
        }

        /*
         * Verify the output mask
         */
        vector<bool> res_check;
        for (int j = 1; j <= nP; ++j) {
            if(j != party) {
                bool check = false;
                for (int i = 0; i < len; i++) {
                    if (party_assignment[i] == party || party_assignment[i] == 0) {
                        block supposed_mac = Delta & select_mask[output_mask_recv[j][i].bit_share? 1 : 0];
                        supposed_mac ^= key->at(j, output_shift + i);

                        block provided_mac = output_mask_recv[j][i].mac;

                        if(!cmpBlock(&supposed_mac, &provided_mac, 1)) {
                            check = true;
                            break;
                        }
                    }
                }
                res_check.push_back(check);
            }
        }
        if(checkCheat(res_check)) error("cheat in FlexOut's output mask!");

        /*
         * Handle the case party_assignment[] = -1
         */
        if(party == ALICE) {
            for(int i = 0; i < len; i++) {
                if(party_assignment[i] == -1) {
                    authenticated_share_results[i].bit_share = value[output_shift + i] ^ masked_output[i];
                    for(int j = 1; j <= nP; j++) {
                        if(j != party) {
                            authenticated_share_results[i].mac[j] = mac->at(j, output_shift + i);
                            authenticated_share_results[i].key[j] = key->at(j, output_shift + i);
                        }
                    }
                }
            }
        } else {
            for(int i = 0; i < len; i++) {
                if(party_assignment[i] == -1) {
                    authenticated_share_results[i].bit_share = value[output_shift + i];
                    for(int j = 1; j <= nP; j++) {
                        if(j != party) {
                            authenticated_share_results[i].mac[j] = mac->at(j, output_shift + i);
                            if(j == ALICE) {
                                authenticated_share_results[i].key[j] =
                                        key->at(j, output_shift + i) ^ (Delta & select_mask[masked_output[i] ? 1 : 0]);
                                // change the MAC key for the first party
                            } else {
                                authenticated_share_results[i].key[j] = key->at(j, output_shift + i);
                            }
                        }
                    }
                }
            }
        }

        //print_block(Delta);

        /*
        cout << "Debug the authenticated output" << endl;
        for(int i = 0; i < 10; i++){
            if(party_assignment[i] == -1) {
                cout << "index: " << i << ", value: " << authenticated_share_results[i].bit_share << endl;
                cout << "mac: " << endl;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        //cout << j << ": ";
                        print_block(authenticated_share_results[i].mac[j]);
                    }
                }
                cout << "key: " << endl;
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        //cout << j << ": ";
                        print_block(authenticated_share_results[i].key[j]);
                        print_block(authenticated_share_results[i].key[j] ^ Delta);
                    }
                }
                cout << "=============" << endl;
            }
        }
         */

        /*
         * Handle the case party_assignment[] = 0 or == party
         */
        for(int i = 0; i < len; i++) {
            if(party_assignment[i] == 0 || party_assignment[i] == party) {
                plaintext_results[i] = value[output_shift + i] ^ masked_output[i];
                for(int j = 1; j <= nP; j++) {
                    if(j != party) {
                        plaintext_results[i] = plaintext_results[i] ^ output_mask_recv[j][i].bit_share;
                    }
                }
            }
        }
    }
};

#endif //EMP_AGMPC_FLEXIBLE_INPUT_OUTPUT_H
