# Contributing to Iris-Headers

Iris-Headers is the shared vocabulary of the Iris family: C++20 headers under `include/` and
`priv/`, the runtime sources under `src/`, their tests, and the CMake and Bazel builds. Other
projects compile these headers into their own translation units, so a change here is a change to
every consumer. This page lists the rules a pull request is held to, as facts of this codebase.
The design they serve, and every decision with its date, is
[development/ARCHITECTURE.md](development/ARCHITECTURE.md); the pipeline's rules are
[development/PIPELINE.md](development/PIPELINE.md). The family's general conventions for code,
plans and CI are kept privately by the maintainers; the review bot's findings name the rule they
apply.

These rules were written 2026-11-27 (ContributingPipelineQuestion_10), from the repository's own rules
adopted 2026-11-24. Each is numbered, so that a review or a commit can cite it.

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DIRIS_HEADERS_BUILD_TYPE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure --no-tests=error   # the type tests
cmake -P cmake/check_runner_labels.cmake                       # the CMake checks (PIPELINE.md)
cmake -P cmake/check_review_configuration.cmake
cmake -P cmake/tests/downstream_verdict_test.cmake
```

The standalone-header check (`IRIS_HEADERS_CHECK_STANDALONE`) is on whenever this repository is
the top-level project, so the first build above runs it too.

## Headers

1. **Every header compiles alone.** Each header includes what it uses and nothing it does not.
   `cmake/standalone_headers.cmake` globs `include/*.hpp` and `priv/*.hpp` and compiles each one
   alone and all of them together, on Linux, macOS and Windows with Clang; a new header is
   checked without editing it. The two headers it excludes, `priv/IrisCorePython.hpp` and
   `priv/IrisCoreVulkan.hpp`, leave the repository in Narrow_3.
2. **No macros for constants, types or functions.** `constexpr`, `inline` functions, scoped
   enumerations and templates express them, because a macro leaks into every consumer's
   translation unit and collides with platform headers such as `<Windows.h>`. The export
   attribute (`IRIS_EXPORT`, set by `IRIS_EXPORT_API`) is the one macro a header carries.
3. **No reserved identifiers**, in any scope the diff touches, include guards included: no name
   begins with an underscore followed by an uppercase letter or contains two underscores in a
   row, and no name at global scope begins with an underscore. The C++ standard reserves them to
   the implementation.

## Consumers

4. **A change to a public type is a change to every consumer.** A type, member, enumeration
   value, layout, signature, or a header's name or location that a consumer compiles changes
   only with Iris-File-Extension and Iris-Slide built against it by the `downstream` cells
   (PIPELINE.md, rule 3). The pull request's "Consumer impact" section says what each consumer
   must change.
5. **A coordinated change has a paired branch of the same name in each consumer.** Iris-Headers
   merges first, and each pull request names the others and the order of the merges, because
   merging this repository alone breaks each consumer's `main` (seen 2026-11-24, when the rename
   to Iris-Slide removed `IrisCodecTypes.hpp` while Iris-File-Extension's matching change was
   still open).
6. **Values that mirror the Iris File Extension specification keep the specification's values.**
   The tile and image encodings, `MetadataType` and the formats equal Iris-File-Extension's
   value for value; a renumbered or reordered value is a breaking change to the file format.
7. **One breaking release** (BreakingReleaseQuestion_4): a breaking change moves every consumer
   in step, with no deprecation shim.

## The vocabulary

8. **Only the shared vocabulary lives here** (VocabularyScopeQuestion_1). A project's own API, a
   graphics API, a binding's glue or another project's dependency pins belong to that project.
9. **The shared types are reused.** `Iris::Result`, `Iris::Buffer`, `Iris::FIFO::Queue`
    (`priv/IrisQueue.hpp`), `Iris::Async::Pool` and `Iris::Memory` (`priv/IrisMemory.hpp`, the memory arena) are the family's; a type that does
    what one of them does is a review finding, and so is a second queue.
10. **Errors.** A function a consumer calls, and a thread's entry, returns an `Iris::Result`; no
    exception crosses it, and it is `noexcept`. A value it produces is written through an output
    reference. Iris-Headers' own failures carry `Iris::Domain::IRIS_HEADERS`, an
    `Iris::HeadersError` code, the offending value and where it failed. Nothing is printed to
    `std::cout` or `std::cerr` in place of returning the failure.
11. **Shared types take the plain name.** A type owned by `std::shared_ptr` is known by the
    pointer alias, and the class it owns carries the suffix `_t`, inside `namespace Iris`
    (`Buffer_t` behind `using Buffer = std::shared_ptr<Buffer_t>;`), so shared ownership is
    visible at every use.
12. **Private helpers** of a header live in a `detail` namespace, since a header has no file
    scope of its own; those of a source file under `src/` are `static`.
13. **Strong indices.** Indices of different things are different instances of one
    `StrongIndex<Tag>` template (StrongIndicesQuestion_5), so one cannot be passed where another
    is expected. Today's `LayerIndex`, `TileIndex` and `ImageIndex` are `uint32_t` aliases until
    StrongIndices_4.2 adds the template and replaces them; no further index is added as an alias.
14. **Code with an external dependency** (SIMD, a platform API) lives in its own source file and
    build target, so a consumer that does not use it neither compiles nor links it. A new
    dependency is under a permissive licence and pinned by version and hash.

## Tests

15. **New behaviour is shown red first.** A change in behaviour comes with a test in the same
    pull request, run failing before the change and passing after it; the pull request quotes
    both runs. The type tests are `tests/types_test.cpp` (Catch2, `IRIS_HEADERS_BUILD_TYPE_TESTS`);
    a CMake script's test is a `cmake -P` script under `cmake/tests/`. A test that records
    existing behaviour says so.

## Identifiers and pull requests

16. **Identifiers are spelled out and stable.** Work items, decisions and requirement rows follow
    the scheme in [development/README.md](development/README.md); an ID is never renumbered or
    reused. Code carries `// RTM <ID>` (C++) or `# RTM <ID>` (CMake, workflows), and a Catch2
    test the tag `[<ID>]`.
17. **One open pull request at a time.** Its branch is `<kind>/<WorkPackage>_<n>`, named before
    the first push, since renaming a pull request's head branch closes the pull request; further
    work goes onto that branch while it is open.
18. **The description follows `.github/pull_request_template.md`.** It names the work items and
    requirement rows the change advances; their ticks, each linking the commit that did the
    work, are kept privately with the work breakdown, and the review bot checks every link. The
    "Agent review" boxes are the bot's.
19. **Nothing private, nothing personal.** This repository is public under MIT: no private
    product, repository, host or cloud project is named, no one is quoted, no path on a
    contributor's machine appears, and no patient data, slide file or secret is committed.
