# `direct<T, Size, Align>`: An inline-storage pimpl vocabulary type

| | |
|---|---|
| Document #: | PxxxxR0 |
| Date: | 2026-07-24 |
| Project: | ISO/IEC JTC1/SC22/WG21 Programming Language C++ |
| Audience: | LEWG |
| Target: | C++29 |
| Reply-to: | [Arthur Ozga](mailto:aozgaa@gmail.com) |

A reference implementation is available as
[beman.direct](https://github.com/bemanproject/direct).

## Abstract

We propose `direct<T, Size, Align>`, a vocabulary type for the pimpl idiom that stores its
contained object inline in a fixed-size byte buffer instead of behind a heap pointer. It
provides the same "`T` may be incomplete in the header" property that makes
`std::unique_ptr`-based and `std::indirect`-based (P3019) pimpl possible, without the
heap allocation both of those approaches require. It complements, and does not replace,
`std::indirect`.

## Motivation

The pimpl idiom hides an implementation type behind a stable ABI boundary by storing it
indirectly and forward-declaring it in the header. Today this is done with
`std::unique_ptr<Impl>`, which works but gives up value semantics (no copy, pointer
identity, an avoidable null state) unless the author hand-writes deep-copy logic.
`std::indirect<T>` (P3019) fixes exactly that: value semantics, deep copy, no null state
in the public API. But `indirect<T>` still heap-allocates its pointee on every
construction — it trades a hand-written deep-copy for a standard vocabulary type, not for
a cheaper pimpl.

For hot-path types where the cost of pimpl is already a concern (small objects
constructed frequently, e.g. per-frame handles, per-request contexts), developers have
long hand-rolled an alternative: reserve a fixed-size byte buffer inline in the class and
placement-construct the implementation type into it — sometimes called the "fast pimpl"
or small-buffer-optimized (SBO) pimpl idiom, described in
[GotW #28](http://www.gotw.ca/gotw/028.htm).

Hand-rolled versions re-derive the same rules each time: which special members may stay
inline in the header and which must move to the `.cpp`; how to defer the
`sizeof(T) <= Size` check past `T`'s incompleteness; how to reuse the storage without
undefined behavior. Getting any of them wrong produces either a compile error at a
confusing location or silent undefined behavior. That combination — a widely used
pattern, a small and stable interface, and failure modes that are easy to get wrong and
hard to detect — is what this proposal addresses.

## Design

### `Size` and `Align` are explicit, not deduced

`T` is permitted to be incomplete at the point `direct<T, Size, Align>` is declared.
`sizeof(T)` and `alignof(T)` are therefore simply
unavailable at that point; there is no way to deduce or default `Size` from `T`. `Align`
defaults to `alignof(std::max_align_t)` for convenience (a safe-but-possibly-wasteful
default), but `Size` has no safe universal default and must always be supplied explicitly
by the author, who is expected to reserve enough room for the largest `Impl` they expect
across every platform/standard-library combination they build for.

### Over-allocation is intended

The requirements are `sizeof(T) <= Size` and `alignof(T) <= Align`, never equality.
Reserving more storage than `T` currently occupies, or requesting stricter alignment
than it currently requires, is a supported use rather than a tolerated mistake.

This allows library implementers to reserve headroom, allowing them to add members up
to the reserved limit without triggering a downstream rebuild.

### Always-engaged value semantics; no valueless state

`std::indirect<T>` has pointer semantics: a move steals the heap pointer, and the source
becomes "valueless" (comparable to a null pointer) because there is nothing else it could
reasonably do — the pointee's storage moved with the pointer. `direct<T, Size, Align>` has
no heap pointer to steal: its "storage" is the object itself. A move can only mean
invoking `T`'s own move constructor/assignment into the fixed buffer, exactly as a plain
`T` data member would. We therefore give `direct<T,...>` always-engaged value semantics:
there is no valueless state, no `valueless_after_move()`, and no engaged flag. A
moved-from `direct<T,...>` holds a moved-from-but-valid `T`.

### No allocator parameter

`indirect<T, Allocator>` parameterizes the allocator used for its one heap allocation.
`direct<T, Size, Align>` never allocates, so there is nothing for an allocator parameter
to control.

### No polymorphism / type erasure

`std::polymorphic<T>` (also P3019) stores a heap-allocated object of a type *derived from*
`T`, discovered and copied via type erasure. `direct<T, Size, Align>` always holds
exactly a `T`.

Since over-allocation is already permitted, the size objection alone does not rule out
storing a `U` derived from `T` that happens to fit the reservation. Three other
properties do, and they are worth stating because the first two look solvable and the
third is not:

- **Locating the `T` subobject.** The observers reach the object with
  `reinterpret_cast<T*>` from the storage. For a `U` whose `T` base is not at offset
  zero — any case where `T` is not the primary base — that address is wrong. Only
  `static_cast<T*>` applied to a `U*` produces the correct pointer, and by the time an
  observer runs, `U` is not known.

  This is the cheapest of the three to fix. What has to be remembered is an offset
  within the storage, not a pointer, so its width is bounded by `Size`: a single byte
  covers any `Size <= 256`, and `Size` is chosen by the author. Nor does computing it
  require the pointer-to-member tricks such an offset usually implies, because a
  `direct` has a live `U` at the moment it needs the value:

  ```cpp
  // At construction, where U is known and the object exists.
  U* u = /* placement-new'd into storage_ */;
  offset_ = static_cast<offset_type>(
      reinterpret_cast<std::byte*>(static_cast<T*>(u)) -
      reinterpret_cast<std::byte*>(u));
  ```

  Every step there is defined: `static_cast` to an accessible base of a live object,
  and a difference between two addresses within one complete object. Contrast the
  compile-time formulations, which need an offset without an object and end up
  ABI-specific — Boost.Intrusive's
  [`parent_from_member.hpp`](https://www.boost.org/doc/libs/1_81_0/boost/intrusive/detail/parent_from_member.hpp)
  carries three implementations selected by compiler, and comments that the
  GCC/Clang one relies on undefined behavior. Since this proposal is not
  `constexpr` anyway, the runtime form is sufficient and avoids all of that.
- **Destruction.** `ptr()->~T()` destroys only the `T` subobject, which is undefined
  for a `U`. This one really is solvable: requiring a virtual destructor on `T` makes
  the call dispatch to `~U`.
- **Copy and move.** `direct(const direct&)` initializes the new object from `*other`,
  whose static type is `T`, so a contained `U` is sliced: the copy holds a `T`. The
  same applies to move, and to both assignment operators. No requirement on `T` fixes
  this, because the operation that must run is selected by the *dynamic* type. The only
  remedy is to store a type-erased set of copy, move, and destroy operations alongside
  the object — which is precisely what `polymorphic` does and `indirect` does not.
  `direct` aims to follow the tradeoffs of `indirect` in this regard.

So the feature is not a small relaxation of the Mandates; it is a different type, with
inline storage and a vtable, whose special members act on the dynamic type. Whether
such a type is worth proposing separately is left open — see
[Open questions](#open-questions) — but it is not reachable by loosening `direct`.

### No `constexpr` support

`indirect<T>` gets constexpr allocation as a special compiler-blessed case for
`std::allocator` (per P3019 and the underlying constexpr-new-expression rules).
`direct<T, Size, Align>` places `T` into a raw `std::byte` buffer via
`reinterpret_cast`/`std::launder`; that pattern is not usable inside a constant expression
under current core language rules, so `direct<T,...>` makes no attempt at `constexpr`
support.

### Completeness requirements

Naming `direct<T, Size, Align>` — as a data member, a type alias, or a function
parameter type — does not require `T` to be complete. Every member function requires
`T` to be complete at the point that member is instantiated.

Each member is only instantiated on first use. This enables the translation unit
actually implementing `T` to be the sole TU that needs its complete definition — the
same discipline `unique_ptr<T>`- and `indirect<T>`-based pimpl already require. Unlike
those, `direct<T,...>` additionally needs a size and alignment to lay out its own
storage; the `sizeof(T) <= Size` and `alignof(T) <= Align` checks are deferred the same
way, and so are diagnosed in that TU rather than in the header.

For the constructors, assignment operators, destructor, and `emplace`, completeness is
inherent: they construct or destroy a `T`. For `operator*` and `operator->` it follows
from `std::launder`, which is specified for complete object types only. This differs
from `unique_ptr<T>`, whose accessors instantiate with `T` incomplete.

### How far the requirement can be enforced

Stating completeness as a Mandates means a violation inside one translation unit is
ill-formed and diagnosed. Nothing stronger is available, and the proposal does not ask
for it.

Whether a type is complete is a property of the point at which the question is asked,
not of the type. A program in which two translation units instantiate the same member
with `T` complete in one and incomplete in the other is therefore ill-formed, no
diagnostic required — the treatment the standard already gives completeness-sensitive
facilities, and the reason repeated proposals for an `is_complete` trait have been
rejected as unimplementable in any useful sense
([std-proposals, 2021](https://lists.isocpp.org/std-proposals/2021/11/3310.php)). This
proposal follows that convention rather than inventing a stronger guarantee it could
not honor.

An implementation is free to improve the message a violation produces, and the
reference implementation does so, since the diagnostics that arise naturally — from
`sizeof` on an incomplete type, or from `std::launder` — name neither `direct` nor the
member at fault. That is quality of implementation. It is not a guarantee this
proposal makes, and no program should depend on the wording or on a violation being
caught at all.

This is a real limitation, not merely a formality. The natural way to expose a
pimpl'd implementation is a one-line accessor in the header:

```cpp
// widget.hpp -- does NOT compile: operator-> needs Impl complete.
struct Data;
class widget {
    std::direct<Data, 64, 8> data_;
  public:
    Data* get() { return data_.operator->(); }
};
```

Because the accessors require completeness, `get` has to be declared in the header
and defined in the translation unit where `Impl` is complete, alongside the special
members that already live there:

```cpp
// widget.hpp
struct Data;
class widget {
    std::direct<Data, 64, 8> data_;
  public:
    Data* get();
};

// widget.cpp -- Impl is complete here
Impl* widget::get() { return impl_.operator->(); }
```

That is the same discipline the special members already require, so it adds no new
rule — but it does mean an accessor cannot be inlined into the header, and callers in
other translation units pay a function call they would not pay with `unique_ptr` (modulo LTO'ing).
Whether that cost is worth removing is discussed under

[Open questions](#open-questions).

### `emplace` and the no-valueless-state obligation

Because `direct<T,...>` has no disengaged state, `emplace` — which must destroy the old
`T` before constructing the new one in the same storage — has nowhere to retreat to if
the new constructor throws. This proposal makes it a precondition that the selected
constructor of `T` not throw: if it does, no `T` remains in the storage and all
subsequent operations on the `direct`, including its destruction, are undefined.
(`std::optional::emplace` handles the same situation by leaving the optional disengaged;
that option is unavailable here.)

Users needing strong-ish behavior with a throwing constructor can write `*d = T(args...)` instead, paying one move.

Alternatives considered: mandating `is_nothrow_constructible_v<T, Args...>` (rejected: overly
restrictive for the common case where the caller knows no throw can occur).
LEWG input on this trade-off would be welcome.

## Retained properties of `T`

The following properties of `T` are observable through `direct<T, Size, Align>`.

- **`noexcept` of each constructor.** Each constructor's exception specification is
  `noexcept(is_nothrow_constructible_v<T, ...>)` for the corresponding argument list.
- **`noexcept` of each assignment operator.**
  `noexcept(is_nothrow_copy_assignable_v<T>)` and
  `noexcept(is_nothrow_move_assignable_v<T>)` respectively.
- **Equality.** `operator==` is defined in terms of `*lhs == *rhs`.
- **Ordering and comparison category.** `operator<=>` returns the synthesized
  three-way result for `T`, preserving `T`'s comparison category.
- **Const propagation.** `const direct` yields `const T&` and `const T*`, as a plain
  `T` member would.
- **Value category on access.** `operator*` is ref-qualified, so accessing an rvalue
  `direct` yields `T&&`.

Evaluation of the exception specifications is deferred to the point each member is
needed, so retaining `noexcept` does not conflict with the completeness requirements
above at the *declaration* site. It does, however, mean that querying one of these
specifications — as `std::vector` does when choosing between moving and copying —
requires `T` to be complete. That is an unstated requirement on an otherwise
incomplete-type-friendly interface, and whether these specifications should depend on
`T` at all is unresolved; see [Open questions](#open-questions).

## Properties of `T` that are not retained

The following properties hold for a `T` data member but not for a `direct<T, Size,
Align>` member. Authors accept these in exchange for the incomplete-`T` and
fixed-layout properties.

- **Trivial copyability.** `is_trivially_copyable_v<direct<T,...>>` is `false` even for
  a trivially copyable `T`, because each special member is user-provided. Consequently
  `std::atomic<direct<T,...>>` is ill-formed, `std::bit_cast` is unavailable, ABIs that
  pass small trivially copyable types in registers pass `direct` by invisible
  reference, and library relocation paths keyed on triviality fall back to element-wise
  moves.
- **Trivial default construction and destruction.** Same cause. `direct` is not an
  implicit-lifetime type.
- **Aggregate status.** `direct` declares constructors, so aggregate and designated
  initialization are unavailable; the `in_place_t` constructor is used instead.
- **Empty-type storage optimization.** An empty `T` as a plain member can occupy no
  storage under `[[no_unique_address]]`. `sizeof(direct<T, Size, Align>)` is `Size`
  regardless of `sizeof(T)`.
- **Constructibility trait accuracy.** `is_default_constructible_v<direct<T,...>>` and
  the copy and move equivalents report `true` whenever the corresponding constructor is
  declared, independent of `T`, because those constructors carry Mandates rather than
  constraints on their declarations. Misuse is diagnosed at instantiation rather than by
  substitution failure, so `direct` is not usable in SFINAE-style dispatch on those
  traits.
- **`T`'s ADL `swap`.** `swap` on two `direct` objects moves through `direct`'s move
  members rather than calling `T`'s own `swap`.
- **`std::hash<T>`.** No `std::hash` specialization is provided for `direct`.
- **Usability in constant expressions.** Placement construction into `std::byte`
  storage and `std::launder` are not permitted during constant evaluation.
- **Destructor exception specification.** `~direct()` is `noexcept`, so a throwing
  `~T()` calls `std::terminate` rather than propagating.

Conditional triviality and the `swap`/`hash` questions are revisited under
[Open questions](#open-questions).

## Comparison

| | `unique_ptr<T>` pimpl | `indirect<T>` | `direct<T, Size, Align>` |
|---|---|---|---|
| Storage | heap | heap | inline |
| Allocation per construction | yes | yes | no |
| Value semantics (copy) | manual | automatic | automatic |
| Move | pointer swap | pointer swap (usually) | invokes `T`'s move ctor |
| Valueless state after move | n/a (nullable by design) | yes | no |
| `T` may be incomplete in header | yes | yes | yes |
| Requires explicit `Size`/`Align` | no | no | yes |
| `constexpr`-friendly | no | yes (allocator-dependent) | no |

## Proposed wording

> Note: the synopsis and per-member semantics below are settled; they will be rendered
> as standardese against the working draft in R1.
>
> Throughout, every member carries the Mandates `sizeof(T) <= Size` and
> `alignof(T) <= Align`, stated once here rather than repeated per member. As discussed
> under [Completeness requirements](#completeness-requirements), these are Mandates on
> the members rather than constraints on the class template so that they are evaluated
> only when a member is instantiated.

Add to `<memory>`, in a new subclause `[direct]` following `[indirect]`:

```cpp
namespace std {
  template <class T, size_t Size, size_t Align = alignof(max_align_t)>
  class direct {
    alignas(Align) byte storage_[Size]; // exposition only
  public:
    using value_type = T;
    static constexpr size_t size = Size;
    static constexpr size_t align = Align;

    direct() noexcept(is_nothrow_default_constructible_v<T>);
    template <class U = T>
      explicit direct(U&&) noexcept(is_nothrow_constructible_v<T, U>);
    template <class... Args>
      explicit direct(in_place_t, Args&&...)
        noexcept(is_nothrow_constructible_v<T, Args...>);
    template <class I, class... Args>
      explicit direct(in_place_t, initializer_list<I>, Args&&...)
        noexcept(is_nothrow_constructible_v<T, initializer_list<I>&, Args...>);
    direct(const direct&) noexcept(is_nothrow_copy_constructible_v<T>);
    direct(direct&&) noexcept(is_nothrow_move_constructible_v<T>);
    direct& operator=(const direct&) noexcept(is_nothrow_copy_assignable_v<T>);
    direct& operator=(direct&&) noexcept(is_nothrow_move_assignable_v<T>);
    ~direct();

    template <class... Args> T& emplace(Args&&...);
    template <class I, class... Args>
      T& emplace(initializer_list<I>, Args&&...);

    T& operator*() &; const T& operator*() const&;
    T&& operator*() &&; const T&& operator*() const&&;
    T* operator->(); const T* operator->() const;

    template <class U, size_t S2, size_t A2>
      friend auto operator==(const direct&, const direct<U, S2, A2>&)
        -> decltype(static_cast<bool>(declval<const T&>() == declval<const U&>()));
    template <class U, size_t S2, size_t A2>
      friend synth-three-way-result<T, U>
        operator<=>(const direct&, const direct<U, S2, A2>&);
  };
}
```

The requirements `sizeof(T) <= Size` and `alignof(T) <= Align` are Mandates on each
member rather than constraints on the class template, so that they are evaluated when a
member is instantiated and not when `direct<T, Size, Align>` is named. The same applies
to the constructibility, assignability, and destructibility requirements on `T`. This
placement is what admits an incomplete `T` at the point of declaration, and it is the
one respect in which the specification cannot follow `indirect`'s.

`Size` shall be greater than zero and `Align` shall be a power of two; `T` shall be a
non-array, non-cv-qualified object type. These constrain only `Size`, `Align`, and the
shape of `T`, so unlike the requirements above they may be checked on the class
template itself.

### Constructors

```cpp
direct() noexcept(is_nothrow_default_constructible_v<T>);                        // (1)
template <class U = T>
  explicit direct(U&& u) noexcept(is_nothrow_constructible_v<T, U>);              // (2)
template <class... Args>
  explicit direct(in_place_t, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>);                             // (3)
template <class I, class... Args>
  explicit direct(in_place_t, initializer_list<I> il, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, initializer_list<I>&, Args...>);       // (4)
direct(const direct& other) noexcept(is_nothrow_copy_constructible_v<T>);         // (5)
direct(direct&& other) noexcept(is_nothrow_move_constructible_v<T>);              // (6)
```

1. *Mandates*: `is_default_constructible_v<T>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with no arguments.
2. *Constraints*: `remove_cvref_t<U>` is neither `direct` nor `in_place_t`.
   *Mandates*: `is_constructible_v<T, U>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with
   `std::forward<U>(u)`.
3. *Mandates*: `is_constructible_v<T, Args...>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with
   `std::forward<Args>(args)...`.
4. *Mandates*: `is_constructible_v<T, initializer_list<I>&, Args...>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with `il`,
   `std::forward<Args>(args)...`.
5. *Mandates*: `is_copy_constructible_v<T>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with `*other`.
6. *Mandates*: `is_move_constructible_v<T>` is `true`.
   *Effects*: Direct-non-list-initializes the contained object with `std::move(*other)`.
   *Postconditions*: `other` holds a `T` in a valid but unspecified state — whatever
   `T`'s move constructor leaves behind. `other` is not valueless; `direct` has no such
   state.

### Destructor

```cpp
~direct();
```

*Mandates*: `is_destructible_v<T>` is `true`.
*Effects*: Destroys the contained object.

### Assignment

```cpp
direct& operator=(const direct& other) noexcept(is_nothrow_copy_assignable_v<T>);  // (1)
direct& operator=(direct&& other) noexcept(is_nothrow_move_assignable_v<T>);       // (2)
```

1. *Mandates*: `is_copy_assignable_v<T>` is `true`.
   *Effects*: If `addressof(other) == this`, no effects. Otherwise equivalent to
   `**this = *other`.
   *Returns*: `*this`.
2. *Mandates*: `is_move_assignable_v<T>` is `true`.
   *Effects*: If `addressof(other) == this`, no effects. Otherwise equivalent to
   `**this = std::move(*other)`.
   *Returns*: `*this`.

Both provide exactly the exception guarantee `T`'s corresponding assignment operator
provides; `direct` performs no defensive copy-and-swap.

### `emplace`

```cpp
template <class... Args> T& emplace(Args&&... args);                              // (1)
template <class I, class... Args>
  T& emplace(initializer_list<I> il, Args&&... args);                             // (2)
```

1. *Mandates*: `is_constructible_v<T, Args...>` is `true`.
2. *Mandates*: `is_constructible_v<T, initializer_list<I>&, Args...>` is `true`.

*Preconditions*: The selected constructor of `T` does not throw an exception. No
argument in `args` refers to the contained object or any subobject of it.

*Effects*: Destroys the contained object, then direct-non-list-initializes a new
contained object with `std::forward<Args>(args)...` for (1), or with `il`,
`std::forward<Args>(args)...` for (2).

*Returns*: A reference to the new contained object.

If the selected constructor exits via an exception, the preconditions are violated and
the behavior is undefined; in particular no object remains in the storage, so
destroying the `direct` is undefined. See
[`emplace` and the no-valueless-state obligation](#emplace-and-the-no-valueless-state-obligation).

### Observers

```cpp
T&        operator*() & noexcept;                                                 // (1)
const T&  operator*() const& noexcept;                                            // (2)
T&&       operator*() && noexcept;                                                // (3)
const T&& operator*() const&& noexcept;                                           // (4)
T*        operator->() noexcept;                                                  // (5)
const T*  operator->() const noexcept;                                            // (6)
```

*Returns*: A reference (1–4) or pointer (5–6) to the contained object, with the
indicated cv- and ref-qualification.

### Comparison operators

```cpp
template <class U, size_t S2, size_t A2>
  friend auto operator==(const direct& lhs, const direct<U, S2, A2>& rhs)
    noexcept(noexcept(*lhs == *rhs))
    -> decltype(static_cast<bool>(*lhs == *rhs));                                 // (1)
template <class U, size_t S2, size_t A2>
  friend synth-three-way-result<T, U>
    operator<=>(const direct& lhs, const direct<U, S2, A2>& rhs);                 // (2)
```

1. *Returns*: `*lhs == *rhs`.
2. *Returns*: `synth-three-way(*lhs, *rhs)`.

These are templates rather than non-template friends deliberately. A non-template
friend whose return type depends on `T` has that type computed when the class is
instantiated, which would require `T` to be complete wherever
`direct<T, Size, Align>` is named — defeating the type's purpose. Being templates
also makes them SFINAE-friendly, so a `T` with no comparison operators removes them
from the overload set rather than relying on their bodies never being instantiated,
and it lets two `direct` objects holding comparable types compare regardless of their
`Size` and `Align`.

A feature-test macro `__cpp_lib_direct` should be added to `<version>` and `<memory>`.

## Usage

The intended use is a class whose implementation type stays incomplete in its header.

```cpp
// widget.hpp -- Impl is incomplete here.
class widget {
    struct Impl;
    std::direct<Impl, 64, 8> impl_;

  public:
    widget();
    ~widget();
    widget(const widget&);
    widget(widget&&) noexcept;
    widget& operator=(const widget&);
    widget& operator=(widget&&) noexcept;
};
```

```cpp
// widget.cpp -- Impl becomes complete here. This is the only translation unit that
// instantiates direct<Impl, 64, 8>'s members, and therefore the only one in which a
// too-small Size or insufficient Align is diagnosed.
struct widget::Impl { /* ... */ };

widget::widget() : impl_(std::in_place) {}
widget::~widget() = default;
widget::widget(const widget&) = default;
widget::widget(widget&&) noexcept = default;
widget& widget::operator=(const widget&) = default;
widget& widget::operator=(widget&&) noexcept = default;
```

`widget`'s special members must be declared but not defined in the header. Defining any
of them there — including `= default` or `inline` — instantiates the corresponding
member of `direct<Impl, 64, 8>` while `Impl` is still incomplete, which is ill-formed.
This is the same discipline `unique_ptr`- and `indirect`-based pimpl require.

Consumers of `widget.hpp` never see `Impl`, and `sizeof(widget)` is fixed by the
reserved storage rather than by `sizeof(Impl)`. If `Impl` outgrows the reservation,
`widget.cpp` fails to compile against the `sizeof(T) <= Size` Mandate; a conforming
diagnostic identifies the member and the two sizes.

## Prior art

### The pimpl idiom

- Herb Sutter, [GotW #24: Compilation Firewalls](http://www.gotw.ca/gotw/024.htm).
  The original statement of the idiom and its compile-time-coupling rationale.
- Herb Sutter, [GotW #100: Compilation Firewalls](https://herbsutter.com/gotw/_100/)
  and [GotW #101: Compilation Firewalls, Part 2](https://herbsutter.com/gotw/_101/).
  The modern `unique_ptr`-based treatment, including the out-of-line
  special-member discipline this proposal also requires.
- [PImpl idiom](https://en.cppreference.com/w/cpp/language/pimpl), cppreference.
- [D-Pointer](https://wiki.qt.io/D-Pointer), Qt Wiki. A large-scale deployment of the
  idiom, with `Q_DECLARE_PRIVATE`/`Q_D` macros standing in for the boilerplate.

### Inline storage for pimpl

- Herb Sutter, [GotW #28: The Fast Pimpl Idiom](http://www.gotw.ca/gotw/028.htm).
  The closest prior treatment of the technique this proposal standardizes: a
  fixed-size inline buffer in place of the heap allocation, with the size checked
  against `sizeof(Impl)` where `Impl` is complete. GotW #28 notes the fragility that
  motivates a library type — the check is easy to write incorrectly or omit, and the
  buffer is easy to under-size.

### Value-semantic wrappers

- [P3019: `indirect` and `polymorphic`](https://wg21.link/P3019), Jonathan Coe,
  Antony Peacock, Sean Parent. The heap-allocating siblings of this type.
- [`std::indirect`](https://en.cppreference.com/w/cpp/memory/indirect), cppreference.
- [beman.indirect](https://github.com/bemanproject/indirect), the Beman implementation
  of P3019.
- Marius Bancila,
  [The pimpl idiom and the C++26 `std::indirect` type](https://mariusbancila.ro/blog/2026/07/23/the-pimpl-idiom-and-the-cpp26-stdindirect-type/).

### Object lifetime and storage reuse

- [`std::launder`](https://en.cppreference.com/w/cpp/utility/launder), cppreference.
  The facility this proposal's accessors depend on, and the reason they require a
  complete `T`.
- [Lifetime](https://en.cppreference.com/w/cpp/language/lifetime), cppreference,
  particularly storage reuse and transparent replaceability.
- [ImplicitLifetimeType](https://en.cppreference.com/w/cpp/named_req/ImplicitLifetimeType),
  cppreference.
- [P0145: Refining Expression Evaluation Order](https://wg21.link/P0145) and
  [P2786: Trivial Relocatability](https://wg21.link/P2786) — relevant to whether a
  future `direct` could be trivially relocatable; see Open questions.
- [P1144: `std::is_trivially_relocatable`](https://wg21.link/P1144), the competing
  relocation proposal.

## Open questions

- **Conditional triviality.** `direct<T,...>` is never `is_trivially_copyable_v` even when
  `T` is, because each special member is user-provided (it must run `static_assert`
  layout checks and go through placement-construct/destroy). A raw `T` member pays no
  such tax. Closing this gap requires a base-class dispatch hierarchy conditioned on `T`'s
  triviality — the same technique `std::optional` implementations use internally. This is
  left as future work; it does not affect runtime cost (construction/copy/move still
  degrade to a single call into `T`'s own possibly-trivial special member), only the
  type-trait classification of `direct<T,...>` itself.

- **An accessor that does not require a complete `T`.** `operator*` and `operator->`
  require completeness only because they launder. `reinterpret_cast<T*>(storage_)`
  alone compiles with an incomplete `T`, so a `T* get() noexcept` that omits the
  launder would restore parity with `unique_ptr`. The difficulty is that accessing the
  contained object through such a pointer is not well-defined: the byte array and the
  contained `T` are not pointer-interconvertible, which is precisely the case
  `std::launder` exists to address. Three directions:

  1. Specify `get()` as returning an unlaundered pointer, with a precondition that the
     caller launder it before use. Sound, but pushes a subtle obligation onto callers
     and is only usable where `T` is complete anyway, which defeats the purpose.
  2. Provide `void* data() noexcept` instead. This trivially admits an incomplete `T`,
     and a caller with a complete `T` writes
     `std::launder(static_cast<T*>(d.data()))`. Sound, and honest about the cost.
  3. Store the pointer returned by placement-new alongside the buffer. This makes every
     accessor incomplete-`T`-friendly with no laundering, at the cost of a pointer per
     object and fix-ups in the copy and move members — which contradicts the
     fixed-layout property that motivates the type.

  We propose none of these for the initial design, but the question is worth
  resolving: it is the only respect in which `direct` is less
  incomplete-`T`-friendly than `unique_ptr`.

- **Opting out of headroom.** Over-allocation is intended (see
  [Over-allocation is intended](#over-allocation-is-intended)), so no diagnostic is
  proposed for it. An author who instead wants the storage to track `T` exactly — say,
  in a type where the reservation is meant to be tight and any slack is a bug — can
  assert it themselves wherever `T` is complete:

  ```cpp
  static_assert(sizeof(widget::Impl) == decltype(widget::impl_)::size,
                "widget::impl_ has slack; shrink the reservation");
  ```

  Whether that is common enough to deserve a named helper is open; nothing in the
  proposal depends on the answer.

- **`swap` and `hash`.** `std::indirect` provides a member `swap` and a `std::hash`
  specialization. This proposal provides neither. `swap` on two `direct` objects already
  works through the move members (three `T` moves, and unlike `indirect` it can never
  leave an operand valueless); `std::hash` could delegate to `std::hash<T>`. Both are
  additive and could be adopted without affecting the rest of the design.

- **Exception specifications that depend on `T`.** The non-template special members
  are specified as `noexcept(is_nothrow_move_constructible_v<T>)` and similar. Those
  specifications are instantiated lazily, so declaring a `direct<T, Size, Align>`
  member with an incomplete `T` is well-formed — but any *query* of the specification
  requires `T` to be complete, and the queries are easy to reach unintentionally.
  `std::vector<Holder>` is enough: `vector` consults
  `is_nothrow_move_constructible_v` to decide between moving and copying on
  reallocation, and the query propagates into `T`.

  So the interface currently has an unstated completeness requirement, reachable from
  a standard container rather than from user code. `indirect` does not have this
  problem: its move constructor is unconditionally `noexcept` because it transfers a
  pointer, and its assignment specification depends on the allocator rather than on
  `T`. `direct` cannot copy that, since its move genuinely runs `T`'s move
  constructor.

  Removing the dependency makes `direct<T, Size, Align>` never
  nothrow-move-constructible, which costs the move-on-reallocation optimization for
  every container of `direct`. Keeping it leaves a requirement that is invisible
  until a container triggers it. A third option is to keep the dependency only on the
  templated constructors, where deferral is genuine, and drop it from the five
  non-template members. We have no recommendation yet; this needs LEWG input on
  whether an unstated completeness requirement of this kind is acceptable, and it is
  the most consequential open question here.
  `tests/beman/direct/fail_nothrow_trait_incomplete.test.cpp` pins the current
  behavior.

- **`emplace` and `noexcept`.** `emplace` currently states a precondition that the
  selected constructor of `T` not throw, leaving the behavior undefined if it does.
  Declaring `emplace` itself `noexcept` would replace that undefined behavior with
  `std::terminate`: identical when the constructor does not throw, defined and
  diagnosable when it does, and it forecloses a `valueless_by_exception` state
  explicitly rather than by omission. The counter-argument is that it converts a
  potentially recoverable situation into process death, and that a caller who knows
  the constructor cannot throw gets nothing from it.

  The broader question is whether it is acceptable for a library type to impose a
  `noexcept` requirement on operations of a user's `T` in order to keep its own
  invariants — a pattern the standard containers already rely on for strong exception
  guarantees, but by *querying* `noexcept` and choosing a strategy rather than by
  *requiring* it. Which of those two models `direct` should follow is unresolved.

- **An inline-storage polymorphic type.** The analysis under
  [No polymorphism / type erasure](#no-polymorphism--type-erasure) shows that storing a
  derived `U` needs a remembered offset and a type-erased operation set — a sibling of
  `polymorphic` with inline storage rather than a relaxation of `direct`. Such a type
  would be a reasonable follow-on proposal: it has the same ABI-headroom motivation and
  the same fixed-layout property, and it would subsume the `fixed_capacity`-style
  small-buffer type erasure that appears repeatedly in practice. It is not proposed
  here because its cost model is different enough to deserve its own discussion.

- **Trivial relocatability.** Even if conditional triviality is not pursued, a `direct`
  holding a trivially relocatable `T` is itself trivially relocatable. Under
  [P2786](https://wg21.link/P2786) or [P1144](https://wg21.link/P1144) this could be
  stated directly, recovering the container-relocation benefit without the base-class
  ladder that conditional triviality requires.
