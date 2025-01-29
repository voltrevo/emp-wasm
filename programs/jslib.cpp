#include <emscripten.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "emp-tool/io/i_raw_io.h"
#include "emp-agmpc/mpc.h"

void run_impl(int party, int nP);

// Implement send_js function to send data from C++ to JavaScript
EM_JS(void, send_js, (int party2, char channel_label, const void* data, size_t len), {
    if (!Module.emp?.io?.send) {
        throw new Error("Module.emp.io.send is not defined in JavaScript.");
    }

    // Copy data from WebAssembly memory to a JavaScript Uint8Array
    const dataArray = HEAPU8.slice(data, data + len);

    Module.emp.io.send(party2 - 1, channel_label, dataArray);
});

// Implement recv_js function to receive data from JavaScript to C++
EM_ASYNC_JS(void, recv_js, (int party2, char channel_label, void* data, size_t len), {
    if (!Module.emp?.io?.recv) {
        reject(new Error("Module.emp.io.recv is not defined in JavaScript."));
        return;
    }

    // Wait for data from JavaScript
    const dataArray = await Module.emp.io.recv(party2 - 1, channel_label, arguments[1]);

    // Copy data from JavaScript Uint8Array to WebAssembly memory
    HEAPU8.set(dataArray, data);
});

class RawIOJS : public IRawIO {
public:
    int party2;
    char channel_label;

    RawIOJS(
        int party2,
        char channel_label
    ):
        party2(party2),
        channel_label(channel_label)
    {}

    void send(const void* data, size_t len) override {
        send_js(party2, channel_label, data, len);
    }

    void recv(void* data, size_t len) override {
        recv_js(party2, channel_label, data, len);
    }

    void flush() override {
        // Ignored for now
    }
};

class MultiIOJS : public IMultiIO {
public:
    int mParty;
    int nP;

    std::vector<emp::IOChannel> a_channels;
    std::vector<emp::IOChannel> b_channels;

    MultiIOJS(int party, int nP) : mParty(party), nP(nP) {
        for (int i = 1; i <= nP; i++) {
            a_channels.emplace_back(std::make_shared<RawIOJS>(i, 'a'));
            b_channels.emplace_back(std::make_shared<RawIOJS>(i, 'b'));
        }
    }

    int size() override {
        return nP;
    }

    int party() override {
        return mParty;
    }

    emp::IOChannel& a_channel(int party2) override {
        assert(party2 != 0);
        assert(party2 != party());

        return a_channels[party2];
    }

    emp::IOChannel& b_channel(int party2) override {
        assert(party2 != 0);
        assert(party2 != party());

        return b_channels[party2];
    }

    void flush(int idx) override {
        assert(idx != 0);

        if (party() < idx)
            a_channels[idx].flush();
        else
            b_channels[idx].flush();
    }
};

EM_JS(char*, get_circuit_raw, (int* lengthPtr), {
    if (!Module.emp?.circuit) {
        throw new Error("Module.emp.circuit is not defined in JavaScript.");
    }

    const circuitString = Module.emp.circuit; // Get the string from JavaScript
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
    if (!Module.emp?.input) {
        throw new Error("Module.emp.input is not defined in JavaScript.");
    }

    const inputBits = Module.emp.input; // Assume this is a Uint8Array

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

EM_JS(int, get_input_bits_start, (), {
    if (!Module.emp?.inputBitsStart) {
        throw new Error("Module.emp.inputBitsStart is not defined in JavaScript.");
    }

    return Module.emp.inputBitsStart;
});

EM_JS(void, handle_output_bits_raw, (uint8_t* outputBits, int length), {
    if (!Module.emp?.handleOutput) {
        throw new Error("Module.emp.handleOutput is not defined in JavaScript.");
    }

    // Copy the output bits to a Uint8Array
    const outputBitsArray = new Uint8Array(Module.HEAPU8.buffer, outputBits, length);

    // Call the JavaScript function with the output bits
    Module.emp.handleOutput(outputBitsArray);
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
    void run(int party, int size) {
        run_impl(party + 1, size);
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

void run_impl(int party, int nP) {
    std::shared_ptr<IMultiIO> io = std::make_shared<MultiIOJS>(party, nP);
    auto circuit = get_circuit();

    auto mpc = CMPC(io, &circuit);

    mpc.function_independent();
    mpc.function_dependent();

    std::vector<bool> input_bits = get_input_bits();
    int input_bits_start = get_input_bits_start();

    FlexIn input(nP, circuit.n1 + circuit.n2, party);

    for (size_t i = 0; i < input_bits.size(); i++) {
        size_t x = i + input_bits_start;
        input.assign_party(x, party);
        input.assign_plaintext_bit(x, input_bits[i]);
    }

    FlexOut output(nP, circuit.n3, party);

    for (int i = 0; i < circuit.n3; i++) {
        // All parties receive the output.
        output.assign_party(i, 0);
    }

    mpc.online(&input, &output);

    std::vector<bool> output_bits;

    for (int i = 0; i < circuit.n3; i++) {
        output_bits.push_back(output.get_plaintext_bit(i));
    }

    handle_output_bits(output_bits);
}

int main() {
    return 0;
}
