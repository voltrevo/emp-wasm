#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

class Buffer {
public:
    // Constructor takes ownership of the allocated data
    Buffer(uint8_t* data, int length);

    // Destructor
    ~Buffer();

    // Move constructor
    Buffer(Buffer&& other) noexcept;

    // Deleted copy constructor and assignment operator to prevent copying
    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;

    // Subscript operators for accessing elements
    uint8_t& operator[](int index);
    const uint8_t& operator[](int index) const;

    // Begin and end iterators for range-based loops and STL compatibility
    uint8_t* begin();
    uint8_t* end();
    const uint8_t* begin() const;
    const uint8_t* end() const;

    // Getters for data and length
    uint8_t* getData();
    const uint8_t* getData() const;
    int getLength() const;

    // Size method to return the number of elements
    int size() const;

    // Empty method to check if the buffer is empty
    bool empty() const;

    // Front and back methods to access the first and last elements
    uint8_t& front();
    const uint8_t& front() const;
    uint8_t& back();
    const uint8_t& back() const;

    // Method to create an explicit copy of the buffer
    Buffer copy() const;

private:
    uint8_t* data;
    int length;
};

#endif // BUFFER_HPP
