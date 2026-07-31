# TypeIt

A terminal touch-typing test. Three bundled difficulties or any text file you point it at, a
15/30/60-second or custom timer, and live words-per-minute and accuracy while you type. Built on
[FTXUI](https://github.com/ArthurSonzogni/FTXUI).

> **Note**: FTXUI does not render ćčšđž. Those characters count as input but show nothing, so if
> your input suddenly reads as incorrect, delete one character before the one you typed.
> [Online test](https://arthursonzogni.github.io/FTXUI/examples/?file=component/input).

## Requirements

- CMake 3.24 or newer, and Ninja
- A C++20 compiler: GCC 13+, Clang 17+, or MSVC 19.38+
- Git

Dependencies are fetched by the build. An installed FTXUI or GoogleTest is used if one is
present, so a distribution build never reaches the network; otherwise both are cloned at a
pinned tag. There is no submodule step.

## Build and run

```bash
git clone https://github.com/KriZa96/TypeIt.git
cd TypeIt
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
./build/linux-gcc-debug/src/TypeIt
```

On Windows, from a developer command prompt:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
.\build\windows-msvc-debug\src\TypeIt.exe
```

Other presets: `linux-gcc-release`, `linux-clang-debug`, `linux-clang-asan` (ASan + UBSan),
`linux-tidy`, `linux-coverage`, `windows-msvc-release`, `windows-clang-cl`, and `vcpkg` for
building through a vcpkg toolchain. See [docs/BUILD.md](docs/BUILD.md).

To keep it on your path:

```bash
sudo ln -sfv "$PWD/build/linux-gcc-debug/src/TypeIt" /usr/games/TypeIt
```

## Playing

Pick a difficulty — or `custom` and a path to any text file — pick a duration, and start typing.
Correct characters turn green, incorrect ones red, and speed and accuracy update as you go.

## Documentation

[docs/](docs/README.md) holds the design and planning documentation for the 2.0 rebuild:

- [CODEBASE_REVIEW.md](docs/CODEBASE_REVIEW.md) — what today's code does well and what is broken
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) — the layering and the decisions behind it, with costs
- [GAMEPLAY.md](docs/GAMEPLAY.md) — modes, the race ramp, exact metric definitions
- [TECHNICAL.md](docs/TECHNICAL.md) — key types, algorithms, schema, config, CLI
- [BUILD.md](docs/BUILD.md) and [CI_CD.md](docs/CI_CD.md) — how it is built and what guards it
- [DIAGRAMS.md](docs/DIAGRAMS.md) — the visual index, if you want the shape of it quickly
- [issues/](docs/issues/README.md) — the backlog, task by task, with acceptance criteria

## Contributing

Branch, keep the build warning-free, add tests for what you change, and run
`ctest --preset linux-gcc-debug` before opening a pull request. The full definition of done is
in [the backlog README](docs/issues/README.md#definition-of-done).

## License

[MIT](LICENSE).
