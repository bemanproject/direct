<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# Development

## Configure and Build the Project Using CMake Presets

The simplest way of configuring and building the project is to use [CMake
Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html). Appropriate
presets for major compilers have been included by default.  You can use `cmake
--list-presets=workflow` to see all available presets.

Here is an example of invoking the `gcc-debug` preset:

```shell
cmake --workflow --preset gcc-debug
```

Generally, there are two kinds of presets, `debug` and `release`.

The `debug` presets are designed to aid development, so they have debuginfo and sanitizers
enabled.

> [!NOTE]
>
> The sanitizers that are enabled vary from compiler to compiler.  See the toolchain files
> under ([`infra/cmake`](infra/cmake/)) to determine the exact configuration used for each
> preset.

The `release` presets are designed for production use, and
consequently have the highest optimization turned on (e.g. `O3`).

## Configure and Build Manually

If the presets are not suitable for your use case, a traditional CMake invocation will
provide more configurability.

To configure, build and test the project manually, you can run this set of commands. Note
that this requires GoogleTest to be installed.

```bash
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=17 \
  # Your extra arguments here.
cmake --build build
ctest --test-dir build
```

> [!IMPORTANT]
>
> Beman projects are [passive projects](
> https://github.com/bemanproject/beman/blob/main/docs/beman_standard.md#cmakepassive_projects),
> so you need to specify the C++ version via `CMAKE_CXX_STANDARD` when manually
> configuring the project.

## Dependency Management

### vcpkg

The best way to install the project's dependencies is to use the vcpkg workflow.

To do so, make sure vcpkg is installed and `VCPKG_ROOT` is defined in your environment,
then specify
`-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`. Vcpkg will handle
the project's dependencies, including GoogleTest.

Example commands:

```shell
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
ctest --test-dir build
```

The file `./vcpkg.json` configures the list of dependencies that will be configured by
vcpkg.

### FetchContent

Instead of installing the project's dependencies via a package manager, you can optionally
configure beman.direct to fetch them automatically via CMake FetchContent.

To do so, specify
`-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake`. This will
bring in GoogleTest automatically along with any other dependency the project may require.

Example commands:

```shell
cmake \
  -B build \
  -S . \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake
cmake --build build
ctest --test-dir build
```

The file `./lockfile.json` configures the list of dependencies and versions that will be
acquired by FetchContent.

## Project-specific configure arguments

Project-specific options are prefixed with `BEMAN_DIRECT`.
You can see the list of available options with:

```bash
cmake -LH -S . -B build | grep "BEMAN_DIRECT" -C 2
```

<details>

<summary>Some project-specific configure arguments</summary>

### `BEMAN_DIRECT_BUILD_TESTS`

Enable building tests and test infrastructure. Default: `ON`.
Values: `{ ON, OFF }`.

### `BEMAN_DIRECT_BUILD_EXAMPLES`

Enable building examples. Default: `ON`. Values: `{ ON, OFF }`.

### `BEMAN_DIRECT_INSTALL_CONFIG_FILE_PACKAGE`

Enable installing the CMake config file package. Default: `ON`.
Values: `{ ON, OFF }`.

This is required so that users of `beman.direct` can use
`find_package(beman.direct)` to locate the library.

</details>

## Editor setup

IntelliSense needs a compilation database. The presets export one to
`build/<preset>/compile_commands.json`, which tooling generally will not find on its
own, since it only searches a file's own directory and its ancestors. Symlink it to the
repository root, where `.gitignore` already expects it:

```bash
ln -sfn build/appleclang-release/compile_commands.json compile_commands.json
```

Re-point it when you switch presets.

Use whichever C++ extension you prefer — just not two at once, or you get two sets of
diagnostics. With clangd, set `"C_Cpp.intelliSenseEngine": "disabled"`; with the
Microsoft C/C++ extension, point `C_Cpp.compileCommands` at the database and skip
clangd. Either can surface `.clang-tidy` findings, once enabled: clangd with
`--clang-tidy`, the Microsoft extension with
`"C_Cpp.codeAnalysis.clangTidy.enabled": true`.

If you use clangd, match its version to the compiler the preset uses (Apple clangd for
the appleclang presets) or expect spurious errors inside standard-library headers. To
check the setup without an editor:

```bash
clangd --compile-commands-dir="$PWD" --check=tests/beman/direct/direct.test.cpp 2>&1 \
  | grep '^E\[' | grep -v 'tweak:'
```

That leaves only real diagnostics. `--check` also logs unavailable refactorings
(`tweak: ... ==> FAIL`) at error severity and counts them in its `N errors` summary, so
the raw count overstates. After building two different configurations, a stale
`.cache/clangd` can produce implausible errors in standard-library headers; delete it.

## Static analysis

`.clang-tidy` selects the check set, and `tests/.clang-tidy` relaxes a few checks that
are wrong for test code — notably the copy-elision advice, where the copy is the subject
of the test. Each exception carries its reason. Batch run:

```bash
clang-tidy -p build/appleclang-release $(git ls-files '*.cpp' '*.hpp' ':!:*fail_*')
```

The `fail_*` fixtures are excluded because they are ill-formed on purpose; analyzing
them reports the very errors they exist to produce. Note that clang-tidy exits 0 for
warnings, so add `--warnings-as-errors='*'` if you want a non-zero status.

`misc-include-cleaner` is on. Includes needed only by an inactive `#if` branch, or whose
contribution is a macro or an operator rather than a name, carry `// IWYU pragma: keep`
with a comment; `detail/config.hpp` marks its generated-header include
`// IWYU pragma: export`, since it is a facade over it.

## Additional verification configurations

Beyond the default preset build, two configurations are worth running before
significant changes (CI covers variants of these on Linux):

### C++17 / no-concepts fallback

Verifies the `BEMAN_DIRECT_USE_CONCEPTS` / `BEMAN_DIRECT_USE_THREE_WAY_COMPARISON`
fallback branches:

```bash
cmake -S . -B build/fallback-check -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=infra/cmake/appleclang-toolchain.cmake \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake \
    -DBEMAN_DIRECT_USE_CONCEPTS=OFF \
    -DBEMAN_DIRECT_USE_THREE_WAY_COMPARISON=OFF
cmake --build build/fallback-check
ctest --test-dir build/fallback-check --output-on-failure
```

(Substitute the toolchain file for your compiler.)

### UndefinedBehaviorSanitizer

The library's placement-new/`std::launder` machinery is exactly the kind of code
UBSan exists for:

```bash
cmake -S . -B build/ubsan -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=infra/cmake/appleclang-toolchain.cmake \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./infra/cmake/use-fetch-content.cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=undefined" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build/ubsan
ctest --test-dir build/ubsan --output-on-failure -E "fail_|compile_success"
```

The `-E` filter skips the negative-compilation harness tests, which recurse into
`cmake --build` and gain nothing from instrumentation. The debug presets'
`BEMAN_BUILDSYS_SANITIZER=MaxSan` adds AddressSanitizer on top of this; note that
some macOS toolchain installs ship a mismatched ASan runtime that hangs at
process start, in which case this UBSan-only configuration is the local
alternative.

### Installed-package check

The installed package must be self-contained. That means both the checked-in
`detail/config.hpp` (packaged via the `HEADERS` file set in
`include/beman/direct/CMakeLists.txt`) and the CMake-*generated*
`detail/config_generated.hpp` (packaged via the file set in the root
`CMakeLists.txt`) — either is easy to silently lose when touching
`target_sources`:

```bash
cmake --install build/appleclang-release --prefix /tmp/beman-direct-install
ls /tmp/beman-direct-install/include/beman/direct/direct.hpp \
   /tmp/beman-direct-install/include/beman/direct/detail/config.hpp \
   /tmp/beman-direct-install/include/beman/direct/detail/config_generated.hpp
```

### Vendored (no-CMake) build

`detail/config.hpp` must keep working when a user copies only `include/`, with
no generated header present ([cpp.no_flag_forking]'s vendoring fallback):

```bash
cp -r include /tmp/vendor-check/ && cd /tmp/vendor-check
c++ -std=c++20 -Iinclude -fsyntax-only -x c++ - <<<'#include <beman/direct/direct.hpp>'
c++ -std=c++17 -DBEMAN_DIRECT_USE_CONCEPTS=0 -DBEMAN_DIRECT_USE_THREE_WAY_COMPARISON=0 \
    -Iinclude -fsyntax-only -x c++ - <<<'#include <beman/direct/direct.hpp>'
```

## Known gaps

### Shared-library boundary tests (TODO)

`tests/beman/direct/` and `examples/` each build the translation unit that
defines `widget::Impl` into its own **static** library, so the pimpl boundary
is a real compile-and-link boundary rather than a convention. What that does
not yet cover is the other half of the motivation for pimpl: ABI stability
across a **shared** library, where the point of the fixed-size inline storage
is that changing `Impl`'s shape and rebuilding the library does not require
rebuilding its consumers.

Adding that needs per-platform symbol visibility — `__declspec(dllexport)` /
`dllimport` on Windows, `-fvisibility=hidden` plus visibility attributes
elsewhere — and the Windows half should be iterated on a Windows machine
rather than guessed at. Until then the static-library tests stand.
