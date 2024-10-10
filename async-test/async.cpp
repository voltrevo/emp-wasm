#include <emscripten.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "emp-ag2pc/2pc.h"

#include "Buffer.hpp"

void start_impl();

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    uint8_t* js_malloc(int size) {
        return (uint8_t*)malloc(size);
    }

    EMSCRIPTEN_KEEPALIVE
    void start() {
        start_impl();
    }
}

EM_ASYNC_JS(uint8_t*, get_data_impl, (int* lengthPtr), {
    console.log("JavaScript: Waiting for data...");

    // Simulate asynchronous data retrieval
    const data = await new Promise((resolve) => {
        Module.provideData = (dataArray) => {
            console.log("JavaScript: Data received.");
            // const dataArray = [1, 2, 3]; // Example data
            resolve(dataArray);
        };
    });

    const len = data.length;
    const dataPtr = Module._js_malloc(len); // Use the exported js_malloc

    // Copy data into the WebAssembly memory
    Module.HEAPU8.set(data, dataPtr);

    // Get the pointer to the length variable in C++
    var lengthPtr = arguments[0];

    // Set the length back to the C++ side
    Module.setValue(lengthPtr, len, 'i32');

    return dataPtr;
});

Buffer get_data() {
    int length;
    uint8_t* data = get_data_impl(&length);
    return Buffer(data, length);
}

int main() {
    return 0;
}

void start_impl() {
    puts("C++: Before calling get_data");

    Buffer data = get_data();

    printf("C++: After calling get_data, data length is %d\n", data.size());

    // Process the data
    for (int i = 0; i < data.size(); ++i) {
        printf("C++: Data[%d] = %d\n", i, data[i]);
    }
}
