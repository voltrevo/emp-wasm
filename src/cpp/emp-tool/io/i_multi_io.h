#ifndef IMULTI_IO_HPP
#define IMULTI_IO_HPP

#include "io_channel.h"

struct IMultiIO {
    virtual int size() = 0;
    virtual int party() = 0;
    virtual emp::IOChannel& a_channel(int party2) = 0;
    virtual emp::IOChannel& b_channel(int party2) = 0;
    virtual void flush(int party2) = 0;
    virtual ~IMultiIO() = default;
};

emp::IOChannel& get_send_channel(IMultiIO& io, int party2) {
    assert(party2 != 0);
    assert(party2 != io.party());

    return io.party() < party2 ? io.a_channel(party2) : io.b_channel(party2);
}

emp::IOChannel& get_recv_channel(IMultiIO& io, int party2) {
    assert(party2 != 0);
    assert(party2 != io.party());

    io.flush(party2);

    return party2 < io.party() ? io.a_channel(party2) : io.b_channel(party2);
}

#endif // IMULTI_IO_HPP
