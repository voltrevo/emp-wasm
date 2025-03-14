#include <emp-tool/emp-tool.h>
#include "emp-tool/io/net_io.h"
#include "emp-ag2pc/2pc.h"
using namespace std;
using namespace emp;

const char* hex_char_to_bin(char c);
std::string hex_to_binary(std::string hex);
std::string binary_to_hex(const std::string& bin);

const string circuit_file_location = "circuits/sha-1.txt";

// can be independently calculated eg with https://xorbin.com/tools/sha1-hash-calculator
const string sha1_empty = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

int main(int argc, char** argv) {
    int port, party;
    parse_party_and_port(argv, &party, &port);

    auto net_io = std::make_shared<NetIO>(party == ALICE ? nullptr : IP, port);

    IOChannel io(net_io);

    string file = circuit_file_location;

    BristolFormat cf(file.c_str());
    auto t1 = clock_start();
    C2PC twopc(io, party, &cf);
    io.flush();
    cout << "one time:\t" << party << "\t" << time_from(t1) << endl;
    t1 = clock_start();
    twopc.function_independent();
    io.flush();
    cout << "inde:\t" << party << "\t" << time_from(t1) << endl;

    t1 = clock_start();
    twopc.function_dependent();
    io.flush();
    cout << "dep:\t" << party << "\t" << time_from(t1) << endl;

    int input_size = party == ALICE ? 512 : 0;
    std::vector<bool> in(input_size);

    if (party == ALICE) {
        // we need a single starting 1 for a valid sha-1 block
        // this will result in sha1("") == da39a3ee5e6b4b0d3255bfef95601890afd80709
        in[0] = true;

        // 512 0   160
        // |   |   ^ 160 output bits
        // |   ^ 0 input bits from Bob
        // ^ 512 input bits from Alice
    }

    t1 = clock_start();
    std::vector<bool> out = twopc.online(in, true);
    cout << "online:\t" << party << "\t" << time_from(t1) << endl;

    string res = "";
    for (int i = 0; i < out.size(); ++i)
        res += (out[i] ? "1" : "0");
    cout << res << endl;
    cout << binary_to_hex(res) << endl;
    cout << sha1_empty << endl;
    cout << (binary_to_hex(res) == string(sha1_empty) ? "GOOD!" : "BAD!") << endl;

    return 0;
}

const char* hex_char_to_bin(char c) {
    switch (tolower(c)) {
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
        case 'a': return "1010";
        case 'b': return "1011";
        case 'c': return "1100";
        case 'd': return "1101";
        case 'e': return "1110";
        case 'f': return "1111";
        default: return "0";
    }
}

std::string hex_to_binary(std::string hex) {
    std::string bin;
    for (unsigned i = 0; i != hex.length(); ++i)
        bin += hex_char_to_bin(hex[i]);
    return bin;
}

std::string binary_to_hex(const std::string& bin) {
    if (bin.length() % 4 != 0) {
        throw std::invalid_argument("Binary string length must be a multiple of 4");
    }

    std::string hex;
    for (std::size_t i = 0; i < bin.length(); i += 4) {
        std::string chunk = bin.substr(i, 4);
        if (chunk == "0000") hex += '0';
        else if (chunk == "0001") hex += '1';
        else if (chunk == "0010") hex += '2';
        else if (chunk == "0011") hex += '3';
        else if (chunk == "0100") hex += '4';
        else if (chunk == "0101") hex += '5';
        else if (chunk == "0110") hex += '6';
        else if (chunk == "0111") hex += '7';
        else if (chunk == "1000") hex += '8';
        else if (chunk == "1001") hex += '9';
        else if (chunk == "1010") hex += 'a';
        else if (chunk == "1011") hex += 'b';
        else if (chunk == "1100") hex += 'c';
        else if (chunk == "1101") hex += 'd';
        else if (chunk == "1110") hex += 'e';
        else if (chunk == "1111") hex += 'f';
        else throw std::invalid_argument("Invalid binary chunk");
    }

    return hex;
}
