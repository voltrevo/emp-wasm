#include "Buffer.hpp"
#include <cstring> // For memcpy

// Constructor takes ownership of the allocated data
Buffer::Buffer(uint8_t* data, int length) : data(data), length(length) {}

// Destructor frees the allocated memory
Buffer::~Buffer() {
    free(data);
}

// Move constructor transfers ownership of resources
Buffer::Buffer(Buffer&& other) noexcept : data(other.data), length(other.length) {
    other.data = nullptr;
    other.length = 0;
}

// Subscript operators for accessing elements
uint8_t& Buffer::operator[](int index) {
    return data[index];
}

const uint8_t& Buffer::operator[](int index) const {
    return data[index];
}

// Begin and end iterators for range-based loops and STL compatibility
uint8_t* Buffer::begin() { return data; }
uint8_t* Buffer::end() { return data + length; }

const uint8_t* Buffer::begin() const { return data; }
const uint8_t* Buffer::end() const { return data + length; }

// Getters for data and length
uint8_t* Buffer::getData() { return data; }
const uint8_t* Buffer::getData() const { return data; }
int Buffer::getLength() const { return length; }

// Size method to return the number of elements
int Buffer::size() const { return length; }

// Empty method to check if the buffer is empty
bool Buffer::empty() const { return length == 0; }

// Front method to access the first element
uint8_t& Buffer::front() {
    if (empty()) throw std::out_of_range("Buffer is empty");
    return data[0];
}

const uint8_t& Buffer::front() const {
    if (empty()) throw std::out_of_range("Buffer is empty");
    return data[0];
}

// Back method to access the last element
uint8_t& Buffer::back() {
    if (empty()) throw std::out_of_range("Buffer is empty");
    return data[length - 1];
}

const uint8_t& Buffer::back() const {
    if (empty()) throw std::out_of_range("Buffer is empty");
    return data[length - 1];
}

// Method to create an explicit copy of the buffer
Buffer Buffer::copy() const {
    // Allocate new memory and copy the data
    uint8_t* newData = static_cast<uint8_t*>(malloc(length));
    if (!newData) {
        throw std::bad_alloc();
    }
    std::memcpy(newData, data, length);

    // Return a new Buffer instance with the copied data
    return Buffer(newData, length);
}
