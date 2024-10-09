/*
MIT License

Copyright (c) 2018 Xiao Wang (wangxiao@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Enquiries about further applications and development opportunities are welcome.
*/

#include <emp-tool/emp-tool.h>
#include "emp-ag2pc/2pc.h"
using namespace std;
using namespace emp;

inline const char* hex_char_to_bin(char c);
inline std::string hex_to_binary(std::string hex);
inline std::string binary_to_hex(const std::string& bin);

const string circuit_file_location = "circuits/sha-1.txt";
static char out3[] = "92b404e556588ced6c1acd4ebf053f6809f73a93";//bafbc2c87c33322603f38e06c3e0f79c1f1b1475";

int main(int argc, char** argv) {
	int port, party;
	parse_party_and_port(argv, &party, &port);

	NetIO* io = new NetIO(party==ALICE ? nullptr:IP, port);
	io->set_nodelay();
	string file = circuit_file_location;

	BristolFormat cf(file.c_str());
	auto t1 = clock_start();
	C2PC twopc(io, party, &cf);
	io->flush();
	cout << "one time:\t"<<party<<"\t" <<time_from(t1)<<endl;
	t1 = clock_start();
	twopc.function_independent();
	io->flush();
	cout << "inde:\t"<<party<<"\t"<<time_from(t1)<<endl;

	t1 = clock_start();
	twopc.function_dependent();
	io->flush();
	cout << "dep:\t"<<party<<"\t"<<time_from(t1)<<endl;


	bool in[512], out[512];
	if(party == ALICE) {
		in[0] = in[1] = true;
		in[2] = in[3] = false;
	} else {
		in[0] = in[2] = true;
		in[1] = in[3] = false;
	}
 	for(int i = 0; i < 512; ++i)
		in[i] = false;
	t1 = clock_start();
	twopc.online(in, out);
	cout << "online:\t"<<party<<"\t"<<time_from(t1)<<endl;
	if(party == BOB){
	string res = "";
		for(int i = 0; i < 160; ++i)
			res += (out[i]?"1":"0");
	//	cout << hex_to_binary(string(out3))<<endl;
		cout << res<<endl;
		cout << binary_to_hex(res)<<endl;
		cout << out3<<endl;
		cout << (binary_to_hex(res) == string(out3)? "GOOD!":"BAD!")<<endl;
	}
	delete io;
	return 0;
}

inline const char* hex_char_to_bin(char c) {
	switch(tolower(c)) {
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

inline std::string hex_to_binary(std::string hex) {
	std::string bin;
	for(unsigned i = 0; i != hex.length(); ++i)
		bin += hex_char_to_bin(hex[i]);
	return bin;
}

inline std::string binary_to_hex(const std::string& bin) {
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
