/**
 * @file IrisMemory.hpp
 * @author Ryan Landvater
 * @brief Virtual Memory Arena (VMA) — cross-platform anonymous and sparse
 *        file-backed memory mapping.
 * @version 0.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026 Iris Digital Pathology (MIT)
 *
 * Provenance: extracted from FastFHIR's FF_Memory (src/FF_memory.cpp,
 * include/FF_Memory.hpp), MPL-2.0, copyright Ryan Landvater; relicensed MIT by
 * the author for inclusion here. The FastFHIR stream machinery (StreamHead,
 * FF_HEADER layout, SHM segments, the "FFHR" magic) is deliberately absent:
 * this is the arena core only — map, sparse-mark, truncate, unmap.
 *
 * \note **No OS headers here.** The implementation lives in src/IrisMemory.cpp
 * so that `windows.h` is not pulled into every translation unit that wants an
 * arena. That is not hygiene for its own sake: `<wingdi.h>` defines PLANES,
 * TRANSPARENT and ERROR as macros, `<winnt.h>` defines IN, OUT and DELETE, and
 * consumers of these headers declare fields by those names — Iris-File-Extension
 * keeps a lint table of the collisions and named LayerExtent::zPlanes around
 * one of them. The handles below are deliberately `void*` and `int` rather than
 * HANDLE and a descriptor type, which is what lets this header stay clean.
 */

#ifndef IRIS_MEMORY_HPP
#define IRIS_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

// The only Iris header this pulls in, and it is here for Result: arena
// creation reports through the ecosystem's error type rather than throwing
// across the boundary. IrisTypes.hpp brings no OS headers, so the "no
// windows.h in a consumer's translation unit" property above is unaffected.
#include "IrisTypes.hpp"

namespace Iris {

/**
 * @brief Shared core of one OS memory mapping.
 *
 * Owned through `Memory`'s shared_ptr: multiple handles, one mapping.
 * The mapping is demand-paged — an anonymous arena costs nothing until
 * touched, and a sparse file-backed arena costs only the pages actually
 * written. Unmaps and closes its handles when the last handle dies, or
 * earlier if a caller invokes `close()`.
 *
 * \warning 64-bit hosts only. The mapping size is a SIZE_T; a multi-GiB
 * arena cannot be addressed from a 32-bit process, and NTFS sparse files
 * require the FSCTL the implementation performs. Consumers needing the
 * portable windowed/remote path should not use this on wasm — see
 * IFE_Window's Emscripten branch for ranged fetch.
 */
class MemoryCore {
public:
    /// Uniform across platforms; the unused handle is simply ignored. Called
    /// only by the factories on Memory. @p read_only marks a mapping whose
    /// file was opened without write permission (truncate_file no-ops on it).
    MemoryCore(std::uint8_t* base, std::size_t capacity, std::string name,
                    void* file_handle, void* map_handle, int fd,
                    bool read_only = false) noexcept;

    ~MemoryCore() noexcept;

    MemoryCore(const MemoryCore&)            = delete;
    MemoryCore& operator=(const MemoryCore&) = delete;

    /// Unmap and release the OS handles. Idempotent; the destructor calls it.
    void close() noexcept;

    /// Shrink (or grow) the backing file. No-op when anonymous or closed.
    void truncate_file(std::size_t size);

    /// Make [offset, offset + size) touchable. See Memory::commit.
    void commit(std::size_t offset, std::size_t size);

    [[nodiscard]] std::uint8_t*      base()     const noexcept { return m_base; }
    [[nodiscard]] std::size_t        capacity() const noexcept { return m_capacity; }
    [[nodiscard]] const std::string& name()     const noexcept { return m_name; }

private:
    std::string   m_name;
    std::size_t   m_capacity = 0;
    std::uint8_t* m_base     = nullptr;
    bool          m_read_only = false;   // opened O_RDONLY: no truncation
#ifdef _WIN32
    void* m_file_handle = nullptr;   // CreateFileA handle (file-backed only)
    void* m_map_handle  = nullptr;   // CreateFileMapping handle
#else
    int   m_fd          = -1;        // open() descriptor (file-backed only)
#endif
};

/**
 * @brief Copyable handle over a shared virtual memory arena.
 *
 * Mirrors FastFHIR's FF_Memory handle/body split: copying a handle shares the
 * same underlying mapping rather than duplicating it.
 */
class Memory {
public:
    Memory() = default;
    explicit Memory(std::shared_ptr<MemoryCore> core) noexcept
        : m_core(std::move(core)) {}

    /// True when this handle refers to a live mapping.
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_core != nullptr;
    }

    /// The mapping's base pointer. Offsets into the arena are relative to it.
    [[nodiscard]] std::uint8_t* base() const noexcept {
        return m_core ? m_core->base() : nullptr;
    }

    /// Total reserved address range.
    [[nodiscard]] std::size_t capacity() const noexcept {
        return m_core ? m_core->capacity() : 0;
    }

    /// Backing path (file-backed) or empty (anonymous).
    [[nodiscard]] std::string name() const noexcept {
        return m_core ? m_core->name() : std::string();
    }

    /**
     * @brief Truncate the backing file to @p size bytes. No-op when anonymous.
     *
     * A writable file-backed arena extends the file to `capacity`, so a caller
     * that maps a large arena and writes less must call this or ship a file
     * whose length is the arena's rather than the payload's. Not cosmetic:
     * an IFE slide records its own length in `FILE_HEADER.FILE_SIZE`, and
     * `validate_file_structure` compares it against the size the OS reports —
     * an untruncated arena-written slide fails its own validation.
     *
     * \warning Truncating below the mapped length leaves the mapping covering
     * bytes past end-of-file; touching those pages raises SIGBUS on POSIX.
     * Call it once writing is finished, with the payload size, and do not
     * write through `base()` afterwards.
     *
     * \warning **Windows refuses this while the file is mapped**:
     * `SetEndOfFile` fails on a file with a mapped view. A writer finishing a
     * file should instead `close()` the arena and then resize the file by
     * path (`std::filesystem::resize_file`) — one order that works on every
     * platform, and leaves the file closed for whoever moves it next.
     *
     * @throws std::system_error if the truncation fails.
     */
    void truncate_file(std::size_t size) const {
        if (m_core) m_core->truncate_file(size);
    }

    /**
     * @brief Release the mapping and its OS handles now, without waiting for
     *        the last handle copy to die. Idempotent.
     *
     * On Windows a file-backed mapping holds the backing file open for the
     * mapping's lifetime, so the file cannot be deleted or its directory
     * removed until every handle is gone. Dropping a `Memory` only
     * unmaps when it happens to hold the final reference; this always does.
     *
     * `base()` returns nullptr afterwards. The handle still tests true — it
     * refers to a core, that core simply no longer owns a mapping.
     */
    void close() const noexcept {
        if (m_core) m_core->close();
    }

    /**
     * @brief Make [@p offset, @p offset + @p size) touchable before first use.
     *
     * An arena is a reservation, not an allocation: nothing is committed until
     * it is needed, which is what lets a caller reserve far more than it will
     * ever write. POSIX and every file-backed arena commit a page on first
     * touch by themselves, so this is a no-op there. A Windows ANONYMOUS arena
     * cannot: its pagefile-backed section is created `SEC_RESERVE` (otherwise
     * the whole reservation is charged to the pagefile up front and a huge one
     * fails), and touching a page that was never committed is an access
     * violation. Call this for every range before writing it — a bump
     * allocator calls it from its claim. Committing an already-committed page
     * is harmless, so concurrent claims that share a page need no coordination.
     *
     * @throws std::out_of_range if the range leaves the arena;
     *         std::system_error if the commit fails.
     */
    void commit(std::size_t offset, std::size_t size) const {
        if (m_core) m_core->commit(offset, size);
    }

private:
    std::shared_ptr<MemoryCore> m_core;
};

/**
 * @brief Parameters for creating a virtual memory arena.
 *
 * One struct rather than a factory per combination, so a new option — a huge
 * page hint, a commit policy — is a field here instead of a fourth entry
 * point. `filepath` empty selects an anonymous arena, the way FastFHIR's
 * FF_MemoryCreateInfo uses a null pointer for the same choice; this takes a
 * path rather than a `const char*` because the arena is a C++ surface and the
 * Windows implementation needs the wide string a `path` can still give it.
 */
struct MemoryCreateInfo {
    /// Reserved address range, and never remapped: the base does not move, so
    /// exhausting it is the caller's terminal error, not a cue to grow. It is a
    /// reservation, not an allocation — anonymous arenas are mapped
    /// `MAP_NORESERVE` / `SEC_RESERVE` so even a huge one is not charged against
    /// memory up front (on Windows, `commit` each range before touching it);
    /// a writable file-backed arena extends the file to it, sparse, so only
    /// written pages occupy disk. Ignored when `read_only` (the mapped length
    /// is the file's own size).
    std::size_t           capacity  = 4ull * 1024 * 1024 * 1024;

    /// Backing file, created if absent. Empty selects an anonymous arena.
    /// Existing content is preserved — a caller needing a clean arena must
    /// truncate or remove the file first.
    std::filesystem::path filepath  = {};

    /// Map an existing file without writing to it: opened O_RDONLY /
    /// GENERIC_READ and mapped PROT_READ / FILE_MAP_READ, never extended and
    /// never marked sparse, so `truncate_file` no-ops as it does on an
    /// anonymous arena. This is what lets an inspector hand a slide to IFE's
    /// validation and abstraction entry points without opening it for
    /// writing — a read-only file, or one a scanner is still writing, maps
    /// exactly as well as a writable one. Requires `filepath`.
    bool                  read_only = false;
};

/**
 * @brief Create a virtual memory arena. @p out_arena is empty on failure.
 *
 * The error boundary for this header: every failure below — a file that
 * cannot be opened, marked sparse, extended or mapped — arrives as a Result
 * carrying the OS diagnostic, and nothing throws past this point. The
 * implementation still raises `std::system_error` internally, because that is
 * where the errno and GetLastError values are; this converts them.
 *
 * Writable file-backed arenas are sparse on both platforms: POSIX `open` +
 * `ftruncate` (grow-only) + `mmap(MAP_SHARED)`, NTFS `CreateFile` +
 * `FSCTL_SET_SPARSE` before mapping, so a multi-GiB arena costs only the pages
 * actually written rather than zeros on disk.
 *
 * An empty file cannot be mapped — both platforms reject a zero-length range —
 * and is not a slide anyway, so a read-only create over one fails.
 */
Result create_memory(const MemoryCreateInfo& info,
                           Memory& out_arena) noexcept;

}  // namespace Iris

#endif  // IRIS_MEMORY_HPP
