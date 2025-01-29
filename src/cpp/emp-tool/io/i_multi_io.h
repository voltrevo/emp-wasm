#ifndef IMULTI_IO_HPP
#define IMULTI_IO_HPP

#include "io_channel.h"

struct IMultiIO {
    virtual int size() = 0;
    virtual int party() = 0;
    virtual emp::IOChannel& send_channel(int party2) = 0;
    virtual emp::IOChannel& recv_channel(int party2) = 0;
    virtual void flush(int party2) = 0;
    virtual ~IMultiIO() = default;
};

#endif // IMULTI_IO_HPP
