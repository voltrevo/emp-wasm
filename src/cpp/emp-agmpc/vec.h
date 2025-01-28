#ifndef VEC_H
#define VEC_H

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <memory>
#include <utility>

// Like std::vector but without <bool> specialization.
// This is important because std::vector<bool> is a bitset which breaks assumptions
// made by code designed for bool* arrays.
template <typename T>
class Vec {
private:
    T* data;          // Raw pointer to the array
    size_t capacity;  // Allocated capacity
    size_t size_;     // Current size

    void grow(size_t new_capacity = 0) {
        if (new_capacity == 0) {
            new_capacity = capacity == 0 ? 1 : capacity * 2;
        }
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data[i]);
        }
        delete[] data;
        data = new_data;
        capacity = new_capacity;
    }

public:
    // Constructors and destructor
    Vec() : data(nullptr), capacity(0), size_(0) {}
    explicit Vec(size_t n, const T& value = T())
        : data(new T[n]), capacity(n), size_(n) {
        for (size_t i = 0; i < n; ++i) {
            data[i] = value;
        }
    }
    ~Vec() { delete[] data; }

    // Copy constructor
    Vec(const Vec& other)
        : data(new T[other.capacity]), capacity(other.capacity), size_(other.size_) {
        for (size_t i = 0; i < size_; ++i) {
            data[i] = other.data[i];
        }
    }

    // Move constructor
    Vec(Vec&& other) noexcept
        : data(other.data), capacity(other.capacity), size_(other.size_) {
        other.data = nullptr;
        other.capacity = 0;
        other.size_ = 0;
    }

    // Copy assignment
    Vec& operator=(const Vec& other) {
        if (this != &other) {
            delete[] data;
            data = new T[other.capacity];
            capacity = other.capacity;
            size_ = other.size_;
            for (size_t i = 0; i < size_; ++i) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    // Move assignment
    Vec& operator=(Vec&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            capacity = other.capacity;
            size_ = other.size_;
            other.data = nullptr;
            other.capacity = 0;
            other.size_ = 0;
        }
        return *this;
    }

    // Accessors
    T& operator[](size_t index) {
        assert(index < size_);
        return data[index];
    }

    const T& operator[](size_t index) const {
        assert(index < size_);
        return data[index];
    }

    T& at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    const T& at(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    // Member functions
    void push_back(const T& value) {
        if (size_ == capacity) {
            grow();
        }
        data[size_++] = value;
    }

    void pop_back() {
        assert(size_ > 0);
        --size_;
    }

    void resize(size_t new_size, const T& value = T()) {
        if (new_size > capacity) {
            grow(new_size);
        }
        if (new_size > size_) {
            for (size_t i = size_; i < new_size; ++i) {
                data[i] = value;
            }
        }
        size_ = new_size;
    }

    size_t size() const { return size_; }
    size_t get_capacity() const { return capacity; }
    bool empty() const { return size_ == 0; }

    void clear() {
        delete[] data;
        data = nullptr;
        capacity = 0;
        size_ = 0;
    }
};

#endif // VEC_H
