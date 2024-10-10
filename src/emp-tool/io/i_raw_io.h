#ifndef IRAW_IO_HPP
#define IRAW_IO_HPP

struct IRawIO {
    virtual void send(const void* data, size_t len) = 0;
    virtual void recv(void* data, size_t len) = 0;
    virtual std::unique_ptr<IRawIO> open_another() = 0;
    virtual void flush() = 0;
    virtual ~IRawIO() = default;
};

#endif // IRAW_IO_HPP
