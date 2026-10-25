# wowdecomp

A C++ decompilation of the World of Warcraft 3.3.5a client (`Wow.exe`, build 12340, June 2010).

Join the discord for other clients and to show off some cool stuff you build.

https://discord.gg/DsZMGUD8aT

Every function is reconstructed from the original binary and keeps the address it came from in an
`// OFFSET:` tag, so each one can be compared against the original. The source tree follows the layout
of the original client (`Engine/Source/<Module>`, `WoW/Source/<Module>`, original file names).

The client builds 64-bit only.

## Progress

The percentages below measure how much of the original binary has a counterpart in this source tree. They do
**not** say how much of the game works, and the missing part is not missing gameplay: the client builds into a
complete executable and the game is 100% playable. Bugs could still be hiding, though; a ported function can behave differently
from the original.

Measured against the 12340 binary on 2026-10-15. A function counts as ported when its address is
tagged in the source.

| | Ported | Total | Share of the original |
|---|---:|---:|---:|
| Functions | 24,515 | 27,157 | 90.3% |
| Code bytes | 5,270,186 | 5,656,573 | 93.2% |

The total leaves out 658 functions that IDA identifies as compiler library code and 132 import thunks.
Most of the untagged remainder is code the build gets elsewhere: compiler-generated code (static
initializers, template instances) that our compiler emits on its own, C runtime helpers, and third-party
libraries that are linked from their upstream sources instead of being decompiled (FreeType, zlib, PCRE,
parts of FMOD Ex).

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with the **Desktop development with C++** workload (MSVC v143 and a Windows SDK)
- CMake 3.10 or newer (the one that ships with Visual Studio works)
- A World of Warcraft **3.3.5a (12340)** client. No game data is included in this repository.

## Building

From a Developer PowerShell or any shell with `cmake` on `PATH`:

```powershell
git clone https://github.com/SatyPardus/wowdecomp.git
cd wowdecomp

cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient --parallel
```

In-source builds are refused, so keep the build directory separate (`build/` above).

The client is written to `bin/Win/`:

| Target | Executable |
|---|---|
| `WowClient` | `WowClient64.exe` |
| `Wow` | `Wow64.exe` |

`WowClient` is the client with a crash logger: it opens a console window and writes
`WowClient_crash.log` when the client crashes. `Wow` is the same client without it. You can also turn the
logger off in `WowClient` with `-DWOWCLIENT_CRASH_LOG=OFF`.

### Linux (native)

The client also builds as a native Linux x86_64 executable. It renders through the Direct3D 9 device on Vulkan, using
[dxvk](https://github.com/doitsujin/dxvk)'s native `libdxvk_d3d9.so` on an SDL2 window, and falls back to the OpenGL
device when no Vulkan driver is found (`SET gxApi "OpenGL"` picks it by hand). Sound goes through FMOD Ex's SDL2
output, and the Windows platform sources are replaced by the `Linux/` ones next to them. Requirements:
clang, lld, CMake, Ninja, SDL2, the OpenGL headers and xvidcore, plus a Vulkan driver to run it. On Debian/Ubuntu:

```sh
sudo apt-get install clang lld cmake ninja-build libsdl2-dev libgl-dev libxvidcore-dev

cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/linux --target Wow
```

The client is written to `bin/Linux/Wow64` (`bin/Linux/HoTClient64` with `-DWOW_OUTPUT_OVERRIDE=HoTClient64`, which
`cross.py` always sets), with `libdxvk_d3d9.so` next to it (CMake downloads the pinned dxvk
release at configure time, the d3d9 headers are in `ThirdParty/dxvk-native`). Copy the `Data` folder of your 3.3.5a client next to it; file names are
matched case-insensitively, so the folder can be copied as-is. Registry settings are kept in
`~/.config/wowdecomp/registry.ini`.

### Cross compiling (Linux and macOS from any host)

`Tools/Cross/cross.py` builds the Linux and macOS clients with clang, lld and Ninja from Windows (or any other host).
It needs Python 3, Git, CMake, LLVM (clang, lld, llvm-ar) and 7-Zip on the PATH. Ninja is taken from the PATH or
from a Visual Studio install.

```sh
python Tools/Cross/cross.py sysroot linux    # Debian bookworm headers and libraries into .cross/linux-x86_64
python Tools/Cross/cross.py sysroot macos    # SDK submodule (sparse), SDL2.framework and xvidcore into .cross/macos

python Tools/Cross/cross.py build linux
python Tools/Cross/cross.py build macos-arm64
python Tools/Cross/cross.py build macos-x86_64

python Tools/Cross/cross.py package macos    # both Mac builds merged into a universal app, bin/HoTClient64-mac.tar.gz
```

The macOS SDK is the `ThirdParty/macos-sdk` submodule. It is marked `update = none` because the repo holds every SDK
since 10.1, so `git submodule update --init` skips it and `sysroot macos` checks out only `MacOSX26.5.sdk`. On
Windows that checkout needs symlink support (Developer Mode). Build trees go to `build/cross-<name>/`. The toolchain
files are in `cmake/toolchains/`. Mac bundles go to `bin/Mac-arm64/` and `bin/Mac-x86_64/`. Every cross build is named
`HoTClient64` (`bin/Linux/HoTClient64`, `HoTClient64.app/Contents/MacOS/HoTClient64`), the name the launcher and the
patch server expect. `--name` on `build` and `package` changes it.

### Options

| CMake option | Default | Effect |
|---|---|---|
| `WOWDECOMP_DISABLE_SIGNATURE_CHECKS` | `OFF` | Skip the MPQ file and `.toc.sig` signature checks, so modified game data and interface files load |
| `WOWCLIENT_CRASH_LOG` | `ON` | Build `WowClient` with the crash logger |
| `WOWCLIENT_FREEZE_DETECTOR` | `ON` | 64-bit `WowClient` force-crashes on the main thread when it hangs, so the dump shows where it was stuck. The `WOWCLIENT_FREEZE_SECONDS` env var changes the 10 second limit, 0 turns it off |
| `WOWCLIENT_OUTPUT_OVERRIDE` | empty | Name the `WowClient` executable something other than `WowClient64` |
| `WOW_OUTPUT_OVERRIDE` | empty | Name the `Wow` executable and Mac bundle something other than `Wow64`. `cross.py` sets `HoTClient64` |

## Running

1. Copy the `Data` folder of your 3.3.5a client into `bin/Win/`, so you have `bin/Win/Data/`.
2. Point `Data/<locale>/realmlist.wtf` at the server you want to connect to.
3. Start `bin/Win/WowClient64.exe` (or `Wow64.exe`).

## Layout

| Path | Contents |
|---|---|
| `Engine/Source/` | Engine modules: Storm, Gx (Direct3D 9 / OpenGL), Model2, Frame/FrameXML UI, SoundEngine, Lua, ... |
| `WoW/Source/` | Game modules: objects, world rendering, UI, networking, database, glue screens |
| `WoW/Common/` | Code shared by the game modules |
| `ThirdParty/` | Libraries linked from their upstream sources |
| `Tools/WowClient/` | The `WowClient` executable target |
| `Tools/XvidDecoder/` | x64 movie decoder that replaces the original 32-bit `DivxDecoder.dll` |

## Special thanks

- [binana](https://github.com/thunderbrewhq/binana)
- [thunderbrew](https://github.com/thunderbrewhq/thunderbrew)
- [whoa](https://github.com/whoahq/whoa)
- TOMRUS, for the initial IDB
- Everyone who has reported bugs so far
- The WoW modding community
