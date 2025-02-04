#ifndef IMULTI_IO_HPP
#define IMULTI_IO_HPP

#include "io_channel.h"

struct IMultiIO {
    virtual int size() = 0;
    virtual int party() = 0;
    virtual emp::IOChannel& a_channel(int other_party) = 0;
    virtual emp::IOChannel& b_channel(int other_party) = 0;
    virtual void flush(int other_party) = 0;
    virtual ~IMultiIO() = default;
};

emp::IOChannel& get_send_channel(IMultiIO& io, int other_party) {
    assert(other_party != 0);
    assert(other_party != io.party());

    return io.party() < other_party ? io.a_channel(other_party) : io.b_channel(other_party);
}

emp::IOChannel& get_recv_channel(IMultiIO& io, int other_party) {
    assert(other_party != 0);
    assert(other_party != io.party());

    io.flush(other_party);

    return other_party < io.party() ? io.a_channel(other_party) : io.b_channel(other_party);
}

#endif // IMULTI_IO_HPP
