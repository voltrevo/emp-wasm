#ifndef NVECTOR_H
#define NVECTOR_H

#include <vector>
#include <stdexcept>
#include <initializer_list>
#include <numeric>
#include <cstddef>
#include <utility>

// N-dimensional vector class
template <typename T>
class NVector {
public:
    // Constructor taking sizes of each dimension
    template <typename... Dims>
    explicit NVector(Dims... dims) {
        static_assert(sizeof...(Dims) > 0, "At least one dimension must be specified.");

        // Store the sizes of each dimension
        dimensions = {static_cast<size_t>(dims)...};

        // Calculate the total size of the data
        total_size = std::accumulate(dimensions.begin(), dimensions.end(), 1ull, std::multiplies<>());

        // Allocate the storage
        data.resize(total_size);
    }

    // Access element with variadic indices
    template <typename... Indices>
    T& at(Indices... indices) {
        static_assert(sizeof...(Indices) > 0, "At least one index must be specified.");
        if (sizeof...(Indices) != dimensions.size()) {
            throw std::out_of_range("Incorrect number of indices.");
        }

        size_t flat_index = compute_flat_index({static_cast<size_t>(indices)...});
        return data.at(flat_index);
    }

    // Const access
    template <typename... Indices>
    const T& at(Indices... indices) const {
        static_assert(sizeof...(Indices) > 0, "At least one index must be specified.");
        if (sizeof...(Indices) != dimensions.size()) {
            throw std::out_of_range("Incorrect number of indices.");
        }

        size_t flat_index = compute_flat_index({static_cast<size_t>(indices)...});
        return data.at(flat_index);
    }

private:
    std::vector<size_t> dimensions; // Sizes of each dimension
    size_t total_size;              // Total size of the data
    std::vector<T> data;            // Linear storage for the elements

    // Compute the flat index from multi-dimensional indices
    size_t compute_flat_index(const std::vector<size_t>& indices) const {
        size_t flat_index = 0;
        size_t stride = 1;

        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (indices[i] >= dimensions[i]) {
                throw std::out_of_range("Index out of bounds.");
            }
            flat_index += indices[i] * stride;
            stride *= dimensions[i];
        }

        return flat_index;
    }
};

#endif // NVECTOR_H
