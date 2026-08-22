# beman.direct: Inline-storage pimpl vocabulary type

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg)](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#the-beman-library-maturity-model)
![Standard Target](https://github.com/bemanproject/beman/blob/main/images/badges/cpp29.svg)
<!-- markdownlint-restore -->

`beman.direct` implements `direct<T, Size, Align>`, a vocabulary type for the pimpl idiom
with inline (non-heap) storage. It provides the same "let `T` stay incomplete in the
header" property as `std::unique_ptr`-based or `std::indirect`-based pimpl, without a heap
allocation per object.

**Implements**: [`direct<T, Size, Align>` (PxxxxR0)](papers/PxxxxR0-beman-direct.md),
a draft paper not yet submitted to WG21.

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)

> [!IMPORTANT]
>
> [The proposal](papers/PxxxxR0-beman-direct.md) is the definitive description of what
> `direct` is and how it behaves — the interface, the semantics of every member, how it
> compares to `std::indirect`, how to choose `Size` and `Align`, and what it does and
> does not preserve about `T`. This README covers only how to build and consume *this
> implementation*.

## License

`beman.direct` is licensed under the Apache License v2.0 with LLVM Exceptions.

## Usage

```cpp
// widget.hpp -- Impl stays incomplete here.
#include <beman/direct/direct.hpp>

class widget {
    struct Impl;
    beman::direct::direct<Impl, 64, 8> impl_;

  public:
    widget();
    ~widget();
    widget(const widget&);
    widget(widget&&) noexcept;
};
```

`Impl` is defined, and `widget`'s members are, in one `.cpp`. See
`examples/` for the complete runnable version, and the proposal's Usage
section for what the pattern requires.

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++ compiler that conforms to the C++20 standard or greater
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_DIRECT_BUILD_TESTS` to
`OFF` when configuring the project.

You can disable building examples by setting CMake option `BEMAN_DIRECT_BUILD_EXAMPLES` to
`OFF` when configuring the project.

## Building

```bash
cmake --preset appleclang-release   # or gcc-release / llvm-release / msvc-release
cmake --build --preset appleclang-release
ctest --preset appleclang-release --output-on-failure
```

See `CONTRIBUTING.md` for the full list of `BEMAN_DIRECT_*` CMake options and for
additional verification configurations.

### Vendoring the headers

`beman.direct` is header-only, so copying the `include/` directory into a C++20
project works without CMake.
