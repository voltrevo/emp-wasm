#include <emp-tool/emp-tool.h>
#include "emp-agmpc/emp-agmpc.h"
using namespace std;
using namespace emp;

// TODO: Restore sha256, 4 participants

const string circuit_file_location = "circuits/adder_32bit.txt";;
static char out3[] = "92b404e556588ced6c1acd4ebf053f6809f73a93";//bafbc2c87c33322603f38e06c3e0f79c1f1b1475";

int main(int argc, char** argv) {
    int port, party;
    parse_party_and_port(argv, &party, &port);

    const static int nP = 2;
    std::shared_ptr<IMultiIO> io = std::make_shared<NetIOMP>(nP, party, port);
    BristolFormat cf(circuit_file_location.c_str());

    CMPC* mpc = new CMPC(io, &cf);
    cout <<"Setup:\t"<<party<<"\n";

    mpc->function_independent();
    cout <<"FUNC_IND:\t"<<party<<"\n";

    mpc->function_dependent();
    cout <<"FUNC_DEP:\t"<<party<<"\n";

    // The split of input into n1 and n2 is meaningless here,
    // what matters is that there are n1+n2 input bits.
    FlexIn input(nP, cf.n1 + cf.n2, party);

    for (int i = 0; i < cf.n1 + cf.n2; i++) {
        if (i < 32) {
            input.assign_party(i, 1);

            if (party == 1) {
                input.assign_plaintext_bit(i, i == 0 || i == 1);
            }
        } else {
            input.assign_party(i, 2);

            if (party == 2) {
                input.assign_plaintext_bit(i, i == 32 || i == 34);
            }
        } //else if (i < 300) {
        //     input.assign_party(i, 3);

        //     if (party == 3) {
        //         input.assign_plaintext_bit(i, false);
        //     }
        // } else {
        //     input.assign_party(i, 4);

        //     if (party == 4) {
        //         input.assign_plaintext_bit(i, false);
        //     }
        // }
    }

    FlexOut output(nP, cf.n3, party);

    for (int i = 0; i < cf.n3; i++) {
        // All parties receive the output.
        output.assign_party(i, 0);
    }

    mpc->online(&input, &output);
    uint64_t band2 = count_multi_io(*io);
    cout <<"bandwidth\t"<<party<<"\t"<<band2<<endl;
    cout <<"ONLINE:\t"<<party<<"\n";

    string res = "";
    for(int i = 0; i < cf.n3; ++i)
        res += (output.get_plaintext_bit(i)?"1":"0");
    cout << hex_to_binary(string(out3))<<endl;
    cout << res<<endl;
    // cout << (res == hex_to_binary(string(out3))? "GOOD!":"BAD!")<<endl<<flush;

    delete mpc;
    return 0;
}
