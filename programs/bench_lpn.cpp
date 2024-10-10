/*
MIT License

Copyright (c) 2018 Xiao Wang (wangxiao1254@gmail.com)

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

#include <iostream>
#include "emp-tool/emp-tool.h"
#include "emp-ot/emp-ot.h"
#include <stdlib.h>
using namespace std;
using namespace emp;

int main(int argc, char** argv) {
    PRG prg;
    int k, n;
    if (argc >= 3) {
        k = atoi(argv[1]);
        n = atoi(argv[2]);
    } else {
        k = 11;
        n = 20;
    }
    if(n > 30 or k > 30) {
        cout <<"Large test size! comment me if you want to run this size\n";
        exit(1);
    }

    block seed;
    block * kk = new block[1<<k];
    block * nn = new block[1<<n];
    prg.random_block(&seed, 1);
    prg.random_block(kk, 1<<k);
    prg.random_block(nn, 1<<n);

    ThreadPool * pool = new ThreadPool(4);
    for (int kkk = 10; kkk < k; ++kkk) {
        auto t1 = clock_start();
        for (int ttt = 0; ttt < 20; ttt++) {
            LpnF2<NetIO, 10> lpn(ALICE, 1<<n, 1<<kkk, pool, nullptr, pool->size());
            lpn.bench(nn, kk);
            kk[0] = nn[0];
        }
        cout << n<<"\t"<<kkk<<"\t";
        cout << time_from(t1)/20<<"\t"<<time_from(t1)/20*1000.0/(1<<n)<<endl;
    }
    cout << nn[0] <<endl;
}
