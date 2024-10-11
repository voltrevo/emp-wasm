#include <emscripten.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "emp-tool/io/i_raw_io.h"
#include "emp-ag2pc/2pc.h"

void run_impl(int party);

// Implement send_js function to send data from C++ to JavaScript
EM_JS(void, send_js, (const void* data, size_t len), {
    if (!Module.send) {
        throw new Error("Module.send is not defined in JavaScript.");
    }

    // Copy data from WebAssembly memory to a JavaScript Uint8Array
    const dataArray = HEAPU8.slice(data, data + len);

    Module.send(dataArray);
});

// Implement recv_js function to receive data from JavaScript to C++
EM_ASYNC_JS(void, recv_js, (void* data, size_t len), {
    if (!Module.recv) {
        reject(new Error("Module.recv is not defined in JavaScript."));
        return;
    }

    // Wait for data from JavaScript
    const dataArray = await Module.recv(arguments[1]);

    // Copy data from JavaScript Uint8Array to WebAssembly memory
    HEAPU8.set(dataArray, data);
});

class RawIOJS : public IRawIO {
public:
    void send(const void* data, size_t len) override {
        send_js(data, len);
    }

    void recv(void* data, size_t len) override {
        recv_js(data, len);
    }

    void flush() override {
        // Ignored for now
    }
};

EM_JS(char*, get_circuit_raw, (int* lengthPtr), {
    if (!Module.getCircuit) {
        throw new Error("Module.getCircuit is not defined in JavaScript.");
    }

    const circuitString = Module.getCircuit(); // Get the string from JavaScript
    const length = lengthBytesUTF8(circuitString) + 1; // Calculate length including the null terminator

    // Allocate memory for the string
    const strPtr = Module._js_char_malloc(length);
    stringToUTF8(circuitString, strPtr, length);

    // Set the length at the provided pointer location
    setValue(lengthPtr, length, 'i32');

    // Return the pointer
    return strPtr;
});

emp::BristolFormat get_circuit() {
    int length = 0;
    char* circuit_raw = get_circuit_raw(&length);

    emp::BristolFormat circuit;
    circuit.from_str(circuit_raw);
    free(circuit_raw);

    return circuit;
}

EM_JS(uint8_t*, get_input_bits_raw, (int* lengthPtr), {
    if (!Module.getInputBits) {
        throw new Error("Module.getInputBits is not defined in JavaScript.");
    }

    const inputBits = Module.getInputBits(); // Assume this returns a Uint8Array

    // Allocate memory for the input array
    const bytePtr = Module._js_malloc(inputBits.length);
    Module.HEAPU8.set(inputBits, bytePtr);

    // Set the length at the provided pointer location
    setValue(lengthPtr, inputBits.length, 'i32');

    // Return the pointer
    return bytePtr;
});

std::vector<bool> get_input_bits() {
    int length = 0;
    uint8_t* input_bits_raw = get_input_bits_raw(&length);

    std::vector<bool> input_bits(length);

    for (int i = 0; i < length; ++i) {
        input_bits[i] = input_bits_raw[i];
    }

    free(input_bits_raw);

    return input_bits;
}

EM_JS(void, handle_output_bits_raw, (uint8_t* outputBits, int length), {
    if (!Module.handleOutputBits) {
        throw new Error("Module.handleOutputBits is not defined in JavaScript.");
    }

    // Copy the output bits to a Uint8Array
    const outputBitsArray = new Uint8Array(Module.HEAPU8.buffer, outputBits, length);

    // Call the JavaScript function with the output bits
    Module.handleOutputBits(outputBitsArray);
});

void handle_output_bits(const std::vector<bool>& output_bits) {
    uint8_t* output_bits_raw = new uint8_t[output_bits.size()];

    for (size_t i = 0; i < output_bits.size(); ++i) {
        output_bits_raw[i] = output_bits[i];
    }

    handle_output_bits_raw(output_bits_raw, output_bits.size());

    delete[] output_bits_raw;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void run(int party) {
        run_impl(party);
    }

    EMSCRIPTEN_KEEPALIVE
    uint8_t* js_malloc(int size) {
        return (uint8_t*)malloc(size);
    }

    EMSCRIPTEN_KEEPALIVE
    char* js_char_malloc(int size) {
        return (char*)malloc(size);
    }
}

void run_impl(int party) {
    auto io = emp::IOChannel(std::make_shared<RawIOJS>());
    auto circuit = get_circuit();

    auto twopc = emp::C2PC(io, party, &circuit);

    twopc.function_independent();
    twopc.function_dependent();

    std::vector<bool> input_bits = get_input_bits();

    std::vector<bool> output_bits = twopc.online(input_bits, true);
    handle_output_bits(output_bits);
}

int main() {
    return 0;
}
