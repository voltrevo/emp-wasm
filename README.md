# emp-wasm

Work in progress compiling authenticated garbling from [emp-toolkit/emp-ag2pc](https://github.com/emp-toolkit/emp-ag2pc) to wasm.

Currently this is just a stripped down version that you can compile with a single clang++ command (see `build.sh`).

# Uncertain Refactors

For most of the changes I'm reasonably confident that I preserved behavior, but there some things I'm less confident about:

- send and recv swapped for bob in `Fpre::generate` (see TODO comment)
- after removing threading, I also moved everything to a single io channel
- rewrite of simd code to not use simd and updated aes usage written by chatgpt o1 preview

Regardless, this version of emp-toolkit needs some checking.
