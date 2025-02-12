# emp-wasm

Wasm build of authenticated garbling from [emp-toolkit/emp-agmpc](https://github.com/emp-toolkit/emp-agmpc).

(If you're not familiar with garbled circuits, you might find [this video](https://www.youtube.com/watch?v=FMZ-HARN0gI) helpful.)

## Usage

Take a look at [mpc-framework](https://github.com/voltrevo/mpc-framework) for a nicer API which utilizes emp-wasm as a dependency. In particular, it allows you to write your circuits in a TypeScript-like language called [summon](https://github.com/voltrevo/summon).

Read on if you're more interested in what's going on under the hood and are prepared to use/write circuits like [these](https://github.com/voltrevo/emp-wasm/blob/main/circuits) directly.

```sh
npm install emp-wasm
```

```ts
import { secureMPC, BufferedIO, IO } from 'emp-wasm';

import circuit from './circuit.ts';

async function main() {
  // emp-wasm has no opinion about how you do your IO, websockets are just used
  // for this example
  const io = await makeWebSocketIO('wss://somehow-connect-to-bob');

  const output = await secureMPC({
    party: 0, // use 1 in the other client
    size: 2, // the number of participants
             // note: this example is a bit 2PC-specific, for a more general
             // example, see internalDemo3 in demo.ts
    circuit, // a string defining the circuit, see circuits/*.txt for examples
    inputBits: Uint8Array.from([/* 0s and 1s defining your input bits */]),
    inputBitsPerParty: [32, 32], // the number of bits contributed by each participant
    io,
    // mode: 'auto', // defaults to auto, but you can force '2pc' mode or 'mpc' mode
  });

  // the output bits from the circuit as a Uint8Array
  console.log(output);
}

async function makeWebSocketIO(url: string, otherParty: number) {
  const sock = new WebSocket(url);
  sock.binaryType = 'arraybuffer';

  await new Promise((resolve, reject) => {
    sock.onopen = resolve;
    sock.onerror = reject;
  });

  // You don't have to use BufferedIO, but it's a bit easier to use, otherwise
  // you need to implement io.recv(len) returning a promise to exactly len
  // bytes
  const io = new BufferedIO(
    otherParty,
    (toParty, channel, data) => {
      assert(toParty === otherParty, 'Unexpected party');
      const channelBuf = new Uint8Array(data.length + 1);

      // emp-wasm requires an a-channel and a b-channel between each pair of
      // participants. We implement this by actually using a single underlying
      // channel and prefixing each message with a byte indicating the channel.
      // BufferedIO assumes this too. If you're not using BufferedIO, you can
      // provide these multiple channels with a method of your choosing.
      channelBuf[0] = channel.charCodeAt(0);

      channelBuf.set(data, 1);
      sock.send(channelBuf);
    },
    () => sock.close(),
  );

  sock.onmessage = (event: MessageEvent) => {
    if (!(event.data instanceof ArrayBuffer)) {
      console.error('Unrecognized event.data');
      return;
    }

    const channelBuf = new Uint8Array(event.data);
    const channel = channelFromByte(channelBuf[0]);

    io.accept(channel, channelBuf.slice(1));
  };

  sock.onerror = (e) => {
    io.emit('error', new Error(`WebSocket error: ${e}`));
  };

  sock.onclose = () => io.close();

  return io;
}

function channelFromByte(byte: number): 'a' | 'b' {
  switch (byte) {
    case 'a'.charCodeAt(0):
      return 'a';
    case 'b'.charCodeAt(0):
      return 'b';
    default:
      throw new Error('Invalid channel');
  }
}

main().catch(console.error);
```

For a concrete example, see `wsDemo` in `demo.ts` (usage instructions further down in readme).

## Demo

```sh
npm install
npm run build
npm run demo
```

Requirements:
- nodejs
- [emscripten](https://emscripten.org/)

### `internalDemo`

If you don't want to juggle multiple pages, you can do `await internalDemo(3, 5)` in the console, which will run two instances in the same page communicating internally.

### `internalDemo3`

This is the only included MPC (>2 parties) demo. MPC mode uses different underlying code, and you can also use MPC mode with the 2PC demos by appending `, 'mpc'` to your parameters. [MPC Framework](https://github.com/voltrevo/mpc-framework) is the intended and higher level API, see that repo for the latest information and demos.

```
> await internalDemo3(1, 2, 3)
< { alice: 6, bob: 6, charlie: 6 }
```

### `consoleDemo`

Open the url in the console in two tabs and run `consoleDemo(0, 3)` in one and `consoleDemo(1, 5)` in the other. This will begin a back-and-forth where each page prints `write(...)` to the console, which you can paste into the other console to send that data to the other instance (note: sometimes there are multiple writes, make sure to copy them over in order). After about 15 rounds you'll get an alert showing `8` (`== 3 + 5`).

### `wsDemo`

Open the url in the console in two tabs and run `await wsDemo(0, 3)` in one and `await wsDemo(1, 5)` in the other. This uses a websocket relay included in `npm run demo` and defined in `scripts/relayServer.ts`.

### `rtcDemo`

Open the url in the console in two tabs and run `await rtcDemo('pair-id', 0, 3)` in one and `await rtcDemo('pair-id', 1, 5)` in the other. This one uses WebRTC (via [peerjs](https://npmjs.com/package/peerjs)) and should work across networks, locating each other via `pair-id`.

## Regular C++ Compile

This library started out as a stripped down version of the original C++ project. You can compile this for your local system and test it like this:

```sh
./scripts/build_mpc_test.sh
./scripts/mpc_test.sh
```

This will calculate `sha1("")==da39a3ee5e6b4b0d3255bfef95601890afd80709`. It proves to the three other participants that Alice (the first party) knows the preimage of this hash. Each side is run in a separate process and they communicate over a local socket.

Requirements:
- clang
- mbedtls (on macos: `brew install mbedtls`)
  - this version of mbedtls is actually *not* needed for the wasm version, since we need to compile a wasm-specific version ourselves

## Uncertain Changes

For most of the changes I'm reasonably confident that I preserved behavior, but there some things I'm less confident about, including:

- send and recv swapped for bob in `Fpre::generate` (see TODO comment)
- after removing threading, I also moved everything to a single io channel
- rewrite of simd code to not use simd and updated aes usage written by chatgpt o1 preview
- used chatgpt o1 preview to migrate from openssl to mbedtls

Regardless, this version of emp-toolkit needs some checking.
