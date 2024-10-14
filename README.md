# emp-wasm

Wasm build of authenticated garbling from [emp-toolkit/emp-ag2pc](https://github.com/emp-toolkit/emp-ag2pc).

(If you're not familiar with garbled circuits, you might find [this video](https://www.youtube.com/watch?v=FMZ-HARN0gI) helpful.)

## Usage

```sh
npm install emp-wasm
```

```ts
import { secure2PC, BufferedIO, IO } from 'emp-wasm';

import circuit from './circuit.ts';

async function main() {
  // emp-wasm has no opinion about how you do your IO, websockets are just used
  // for this example
  const io = await makeWebSocketIO('wss://somehow-connect-to-bob');

  const output = await secure2PC(
    'alice', // use 'bob' in the other client
    circuit, // a string defining the circuit, see circuits/*.txt for examples
    Uint8Array.from([/* 0s and 1s defining your input bits */]),
    io,
  );

  // the output bits from the circuit as a Uint8Array
  console.log(output);
}

async function makeWebSocketIO(url: string): Promise<IO> {
  const sock = new WebSocket('wss://somehow-talk-to-bob');
  sock.binaryType = 'arraybuffer';

  const openPromise = new Promise(resolve => {
    sock.onopen = resolve;
  });

  const errorPromise = new Promise<never>((_resolve, reject) => {
    sock.onerror = reject;
  });

  await Promise.race([openPromise, errorPromise]);

  // You don't have to use BufferedIO, but it's a bit easier to use, otherwise
  // you need to implement io.recv(len) returning a promise to exactly len
  // bytes
  const io = new BufferedIO(
    (data: Uint8Array) => {
      sock.send(data);
    },
  );

  sock.onmessage = (event: MessageEvent) => {
    if (!(event.data instanceof ArrayBuffer)) {
      console.error('Unrecognized event.data');
      return;
    }

    // Pass Uint8Arrays to io.accept
    io.accept(new Uint8Array(event.data));
  };

  sock.onerror = (e) => {
    io.emit('error', new Error(`WebSocket error: ${e}`));
  };

  return io;
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
