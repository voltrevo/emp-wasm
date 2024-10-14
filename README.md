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
npm install
npm run build
```

This generates `build/index.html` and some assets. Run it using `npx live-server build`.

Currently it's a rather unpolished environment, but you can open this in two tabs and paste `dist/src/ts/consoleCode.js` into each console.

From there run `simpleDemo('alice', 3)` in one tab and `simpleDemo('bob', 5)` in the other. This will begin a back-and-forth where each page prints `write(...)` to the console, which you can paste into the other console to send that data to the other instance. After about 15 rounds you'll get an alert showing `8` (`== 3 + 5`).

Requirements:
- nodejs
- [emscripten](https://emscripten.org/)
- static http server

## Uncertain Changes

For most of the changes I'm reasonably confident that I preserved behavior, but there some things I'm less confident about:

- send and recv swapped for bob in `Fpre::generate` (see TODO comment)
- after removing threading, I also moved everything to a single io channel
- rewrite of simd code to not use simd and updated aes usage written by chatgpt o1 preview
- used chatgpt o1 preview to migrate from openssl to mbedtls

Regardless, this version of emp-toolkit needs some checking.
