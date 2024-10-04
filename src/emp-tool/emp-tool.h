#include <algorithm>
using std::min;

#include <thread>
#include "emp-tool/io/io_channel.h"
#include "emp-tool/io/net_io_channel.h"

#include "emp-tool/circuits/circuit_file.h"

#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
#include "emp-tool/utils/hash.h"
#include "emp-tool/utils/prg.h"
#include "emp-tool/utils/prp.h"
#include "emp-tool/utils/utils.h"
#include "emp-tool/utils/ThreadPool.h"
#include "emp-tool/utils/group.h"
#include "emp-tool/utils/mitccrh.h"
#include "emp-tool/utils/aes_opt.h"
#include "emp-tool/utils/aes.h"
#include "emp-tool/utils/f2k.h"

#include "emp-tool/gc/halfgate_eva.h"
#include "emp-tool/gc/halfgate_gen.h"

#include "emp-tool/execution/circuit_execution.h"
#include "emp-tool/execution/protocol_execution.h"
