# emp-wasm

Wasm build of authenticated garbling from [emp-toolkit/emp-ag2pc](https://github.com/emp-toolkit/emp-ag2pc).

This is also a stripped down version that you can compile with a single clang++ command (see quick-start scripts).

## Quick Start

You can compile the stripped down version and run a test like this:

```sh
./scripts/build_local_test.sh
./scripts/local_test.sh
```

This will calculate `sha1("")==da39a3ee5e6b4b0d3255bfef95601890afd80709`. It proves to Alice that Bob knows the preimage of this hash. Each side is run in a separate process and they communicate over a local socket.

Requirements:
- clang
- mbedtls (on macos: `brew install mbedtls`)
  - this version of mbedtls is actually *not* needed for the wasm version, since we need to compile a wasm-specific version ourselves

## Wasm

```sh
./scripts/build_mbedtls.sh
./scripts/build_wasm.sh
```

This generates `build/index.html` and some assets. To run it you'll need an http static file server, if you have nodejs, you can use `npx live-server build`.

Currently it's a rather unpolished environment, but you can open this in two tabs and paste `src/consoleCode.js` into each console.

From there run `Module._run(1)` in one tab and `Module._run(2)` in the other. This will begin a back-and-forth where each page prints `write(...)` to the console, which you can paste into the other console to send that data to the other instance. Eventually the output will be displayed, which should be `[0, 0, 0, 0, ...]`. This is 0 in little endian, the successful sum of 0 and 0. To change the numbers provided by each party, see `getInputBits`.

Requirements:
- [emscripten](https://emscripten.org/)
- static http server

## Uncertain Changes

For most of the changes I'm reasonably confident that I preserved behavior, but there some things I'm less confident about:

- send and recv swapped for bob in `Fpre::generate` (see TODO comment)
- after removing threading, I also moved everything to a single io channel
- rewrite of simd code to not use simd and updated aes usage written by chatgpt o1 preview
- used chatgpt o1 preview to migrate from openssl to mbedtls

Regardless, this version of emp-toolkit needs some checking.
