#ifndef CMPC_CONFIG
#define CMPC_CONFIG
const static int abit_block_size = 1024;
const static int fpre_threads = 1;
#define LOCALHOST

#ifdef __clang__
    #define __MORE_FLUSH
#endif

//#define __debug
const static char *IPs[] = {""
,    "127.0.0.1"
,    "127.0.0.1"
,    "127.0.0.1"
,    "127.0.0.1"};

#endif // CMPC_CONFIG
