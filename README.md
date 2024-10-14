# emp-wasm

Wasm build of authenticated garbling from [emp-toolkit/emp-ag2pc](https://github.com/emp-toolkit/emp-ag2pc).

(If you're not familiar with garbled circuits, you might find [this video](https://www.youtube.com/watch?v=FMZ-HARN0gI) helpful.)

## Usage

```sh
npm install emp-wasm
```

```ts
import { secure2PC, BufferedIO } from 'emp-wasm';

import circuit from './circuit.ts';

async function main() {
  const io = new BufferedIO(
    (data: Uint8Array) => {
      // send data to Bob
    },
  );

  const output = await secure2PC(
    'alice', // use 'bob' in the other client
    circuit, // a string defining the circuit, see circuits/*.txt for examples
    Uint8Array.from([/* 0s and 1s defining your input bits */]),
    io, // or implement { send, recv } directly (requires some buffer bookkeeping)
  );

  onReceivedData((data: Uint8Array) => {
    // When you receive data from Bob, pass it to io.accept
    io.accept(data);
  });

  // the output bits from the circuit as a Uint8Array
  console.log(output);
}

main().catch(console.error);
```

## Regular C++ Compile

This library started out as a stripped down version of the original C++ project. You can compile this for your local system and test it like this:

```sh
./scripts/build_local_test.sh
./scripts/local_test.sh
```

This will calculate `sha1("")==da39a3ee5e6b4b0d3255bfef95601890afd80709`. It proves to Alice that Bob knows the preimage of this hash. Each side is run in a separate process and they communicate over a local socket.

Requirements:
- clang
- mbedtls (on macos: `brew install mbedtls`)
  - this version of mbedtls is actually *not* needed for the wasm version, since we need to compile a wasm-specific version ourselves

## Console Demo

```sh
npm install
npm run build
npm run demo
```

The demo is a local webserver enabling console interaction.

Open the url in the console in two tabs and run `simpleDemo('alice', 3)` in one and `simpleDemo('bob', 5)` in the other. This will begin a back-and-forth where each page prints `write(...)` to the console, which you can paste into the other console to send that data to the other instance (note: sometimes there are multiple writes, make sure to copy them over in order). After about 15 rounds you'll get an alert showing `8` (`== 3 + 5`).

Requirements:
- nodejs
- [emscripten](https://emscripten.org/)

## Uncertain Changes

For most of the changes I'm reasonably confident that I preserved behavior, but there some things I'm less confident about:

- send and recv swapped for bob in `Fpre::generate` (see TODO comment)
- after removing threading, I also moved everything to a single io channel
- rewrite of simd code to not use simd and updated aes usage written by chatgpt o1 preview
- used chatgpt o1 preview to migrate from openssl to mbedtls

Regardless, this version of emp-toolkit needs some checking.
