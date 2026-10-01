# NativeSharp compiler third-party sources

These sources are vendored so the Windows x64 compiler can be built reproducibly without
downloading dependencies and without linking `LLVM-C.dll` or dynamic compression runtimes.

| Dependency | Version | Upstream archive | SHA-256 |
| --- | --- | --- | --- |
| zlib | 1.3.2 | `https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz` | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` |
| zstd | 1.5.7 | `https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz` | `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3` |

Only the library sources and upstream notices needed by NativeSharp are retained. The build is
defined by `cmake/NativeSharpLlvmStaticDependencies.cmake`; upstream programs, tests and shared
library targets are intentionally excluded. The original licenses remain at `zlib/LICENSE`,
`zstd/LICENSE`, and `zstd/COPYING`.
