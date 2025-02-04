#include <emp-tool/emp-tool.h>
#include "emp-agmpc/emp-agmpc.h"
using namespace std;
using namespace emp;

const string circuit_file_location = "circuits/sha-1.txt";;
const string sha1_empty = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

int main(int argc, char** argv) {
    int port, party;
    parse_party_and_port(argv, &party, &port);

    const static int nP = 4;
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
        input.assign_party(i, 1);

        if (party == 1) {
            if (i == 0) {
                // We need a single starting 1 for a valid sha-1 block.
                // This will result in sha1("") == da39a3ee5e6b4b0d3255bfef95601890afd80709.
                input.assign_plaintext_bit(i, true);
            } else {
                input.assign_plaintext_bit(i, false);
            }
        }
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
    cout << hex_to_binary(sha1_empty)<<endl;
    cout << res<<endl;
    cout << (res == hex_to_binary(sha1_empty)? "GOOD!":"BAD!")<<endl<<flush;

    delete mpc;
    return 0;
}
