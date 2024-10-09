#include <emscripten.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    uint8_t* js_malloc(int size) {
        return (uint8_t*)malloc(size);
    }
}

EM_ASYNC_JS(uint8_t*, get_data, (int* lengthPtr), {
    console.log("JavaScript: Waiting for data...");

    // Simulate asynchronous data retrieval
    const data = await new Promise((resolve) => {
        window.provideData = (dataArray) => {
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

int main() {
    puts("C++: Before calling get_data");

    int length = 0;
    uint8_t* data = get_data(&length);

    printf("C++: After calling get_data, data length is %d\n", length);

    // Process the data
    for (int i = 0; i < length; ++i) {
        printf("C++: Data[%d] = %d\n", i, data[i]);
    }

    // Free the allocated memory
    free(data);

    return 0;
}