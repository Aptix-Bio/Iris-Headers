//
//  IrisQueue.hpp
//  Iris
//
//  Created by Ryan Landvater on 12/10/23.
//  Rebuilt 2026-11-12 (AsyncRebuilt_4.12, development/MODERNIZATION.md).
//

#ifndef IrisQueue_h
#define IrisQueue_h

// Self-contained: compiles with no other header first (StandaloneHeaderBuild_1.1).
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Iris {
/**
 * @brief The Iris queue: an unbounded FIFO for any number of producers and
 * consumers, in which no operation waits for another thread.
 *
 * The protocol is the one Iris has always used (IrisV2's image slots, FIFO2's
 * entries): storage is fixed entries, each with an atomic state, and a thread
 * takes an entry by compare-and-swap on that state. A consumer that loses the
 * compare-and-swap moves on to the next entry.
 *
 *     FREE --producer writes--> PENDING --consumer's compare-and-swap--> CLAIMED
 *
 * - Entries live in segments of `Entries` each, linked oldest to newest. A
 *   producer takes the next index of the newest segment with one fetch-add,
 *   writes its value and stores PENDING. A consumer walks the oldest segment
 *   with its own cursor (FastFHIR's FF_Queue, and FIFO2's iterator before it),
 *   so consumers share nothing but the entries they compete for.
 * - Indices only count up within a segment and a segment is refilled only
 *   after a full reset, so there is no index that wraps onto live entries (the
 *   hazard of a ring, and of FIFO2's 16-bit front).
 * - A segment whose entries are all taken is reset and linked back as the
 *   newest, so a steady flow allocates nothing. Segments are freed only by the
 *   queue's destructor: the memory a thread reaches through a stale pointer is
 *   always a live Segment.
 * - A thread reads or writes a segment's entries only while it holds a pin on
 *   it: it increments the segment's pin count, then checks that the queue
 *   still names that segment (pin()). A segment is reset only once the queue
 *   no longer names it and its pin count has reached zero, by whichever thread
 *   releases the last pin. FIFO2 loaded the pointer, then incremented a count
 *   in memory that could already be freed (QueueManyProducers_4.11); here the
 *   increment lands in memory that is never freed, and the check after it
 *   discards a pointer that went stale. Michael's hazard pointers work the
 *   same way, publishing the pointer and then validating it (IEEE TPDS 15(6),
 *   2004, https://doi.org/10.1109/TPDS.2004.8; citation and the
 *   publish-then-validate step checked 2026-11-12); a count in the segment
 *   replaces his per-thread records because segments are recycled instead of
 *   freed.
 *
 * Guarantees:
 * - Every pushed value is popped exactly once, or destroyed with the queue.
 * - With one consumer, each producer's values arrive in the order it pushed
 *   them. With several consumers the order between consumers is not defined.
 * - push and pop never block and never spin on another thread's progress.
 *   pop returns nothing when the oldest unclaimed entry is not yet written:
 *   the queue is empty, or a producer has taken that index and has not stored
 *   PENDING. The entry appears at a later pop.
 * - push allocates only when every segment is in use (the queue has grown).
 *
 * Ordering a caller may rely on: storing PENDING and linking a new segment are
 * sequentially consistent operations. A thread that must not miss a push it
 * races with (a consumer about to sleep) issues a sequentially consistent
 * fence before its pop; the pop then sees every push that completed before
 * the fence (C++20 [atomics.order] p4, bullet 4.3:
 * https://timsong-cpp.github.io/cppwp/n4861/atomics.order#4, read 2026-11-12).
 */
namespace FIFO {

/// Cache line, so producers' and consumers' counters never share one: 128
/// bytes on Apple silicon (`sysctl hw.cachelinesize`, measured 2026-11-12),
/// and 64 on x86-64, which 128 also separates.
inline constexpr std::size_t kCacheLine = 128;

/// Entries in one segment, by default. Each time a segment fills or is used
/// up, every cursor on it pins the next one: with 8 producers and 8 consumers
/// that cost held the queue to 15 M entries/s at 256 entries and 29 M/s at
/// 2048 (measured 2026-11-12, M5 Pro). FF_Queue uses 2000.
inline constexpr std::size_t kSegmentEntries = 2048;

template <class T, std::size_t Entries = kSegmentEntries>
    requires std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T> && (Entries > 0)
class Queue {
    struct Segment;

public:
    /// `reserve`: values the queue can hold before push first allocates.
    explicit Queue(const std::size_t reserve = 0) {
        std::vector<std::unique_ptr<Segment>> chain;
        for (std::size_t held = 0; chain.empty() || held < reserve; held += Entries)
            chain.push_back(std::make_unique<Segment>());
        for (std::size_t index = 1; index < chain.size(); ++index)
            chain[index - 1]->next.store(chain[index].get(), std::memory_order_relaxed);
        _segments.store(chain.size(), std::memory_order_relaxed);
        _head.store(chain.front().get());
        _tail.store(chain.front().get());
        _last.store(chain.back().get());
        for (std::unique_ptr<Segment>& segment : chain) static_cast<void>(segment.release());  // the chain owns them now
    }
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue&&) = delete;

    /// Pre: no thread is in push or pop and no cursor is alive. Every segment is then in the chain:
    /// a retired one is linked back by whoever released its last pin.
    ~Queue() {
        std::unique_ptr<Segment> segment(_head.load());
        while (segment != nullptr) {
            for (Entry& entry : segment->entries)
                if (entry.state.load(std::memory_order_relaxed) == State::PENDING) std::destroy_at(entry.value());
            segment.reset(segment->next.load(std::memory_order_relaxed));
        }
    }

    /// Any thread, with no cursor: pins the newest segment for this one push.
    /// Throws std::bad_alloc, leaving `value` untouched, when the queue must
    /// grow and cannot.
    template <class U>
        requires std::is_nothrow_constructible_v<T, U&&>
    void push(U&& value) {
        Producer(*this).push(std::forward<U>(value));
    }

    /// A producer's cursor, borrowed from the queue: it keeps the newest
    /// segment pinned between pushes, so a push is one fetch-add and one
    /// store. One thread uses a cursor.
    class Producer {
    public:
        /// Pre: `queue` outlives this cursor.
        explicit Producer(Queue& queue) noexcept : _queue(queue) {}
        Producer(const Producer&) = delete;
        Producer& operator=(const Producer&) = delete;
        ~Producer() { release(); }

        /// As Queue::push.
        template <class U>
            requires std::is_nothrow_constructible_v<T, U&&>
        void push(U&& value) {
            _queue.write(_segment, std::forward<U>(value));
        }

        /// Lets go of the pinned segment; the next push pins again.
        void release() noexcept {
            if (_segment != nullptr) _queue.release(std::exchange(_segment, nullptr));
        }

    private:
        Queue& _queue;
        Segment* _segment = nullptr;
    };

    /// A consumer's cursor, borrowed from the queue: it keeps the oldest
    /// segment pinned and its own place in it, so a pop is one
    /// compare-and-swap on the entry and touches nothing other consumers
    /// share. One thread uses a cursor. Every consumer pops through one.
    ///
    /// An idle cursor (producer's or consumer's) keeps at most one used-up
    /// segment from being recycled and holds no other thread back; release()
    /// gives that up.
    class Consumer {
    public:
        /// Pre: `queue` outlives this cursor.
        explicit Consumer(Queue& queue) noexcept : _queue(queue) {}
        Consumer(const Consumer&) = delete;
        Consumer& operator=(const Consumer&) = delete;
        ~Consumer() { release(); }

        /// The oldest value no other consumer has claimed, or nothing.
        [[nodiscard]] std::optional<T> pop() noexcept { return _queue.take(_segment, _index); }

        /// Lets go of the pinned segment; the next pop pins again.
        void release() noexcept {
            if (_segment != nullptr) _queue.leave(std::exchange(_segment, nullptr), _index);
        }

    private:
        Queue& _queue;
        Segment* _segment = nullptr;
        std::uint64_t _index = 0;  // every entry below it in _segment is claimed
    };

    /// Segments this queue has allocated; it never frees one before its destructor.
    [[nodiscard]] std::size_t segments() const noexcept { return _segments.load(std::memory_order_relaxed); }

private:
    enum class State : std::uint8_t { FREE, PENDING, CLAIMED };

    struct Entry {
        std::atomic<State> state{State::FREE};
        alignas(T) std::array<std::byte, sizeof(T)> storage;
        /// Pre: the caller wrote the value, or claimed it.
        T* value() noexcept { return std::launder(reinterpret_cast<T*>(storage.data())); }
    };

    struct Segment {
        /// Twice the number of threads holding this segment, plus kRetired.
        alignas(kCacheLine) std::atomic<std::uint64_t> pins{0};
        /// Producers: indices handed out.
        alignas(kCacheLine) std::atomic<std::uint64_t> written{0};
        /// Consumers: every entry below it is claimed. A hint for where a
        /// cursor starts, raised when a cursor lets go.
        alignas(kCacheLine) std::atomic<std::uint64_t> skip{0};
        std::atomic<Segment*> next{nullptr};
        alignas(kCacheLine) std::array<Entry, Entries> entries;
    };

    static constexpr std::uint64_t kRetired = 1;
    static constexpr std::uint64_t kPin = 2;

    /// The segment `root` names, pinned. The count is raised first, in memory
    /// that is never freed; `root` still naming the segment afterwards proves
    /// it was not retired before the pin, so it is not reset until the release.
    Segment* pin(std::atomic<Segment*>& root) noexcept {
        for (;;) {
            Segment* segment = root.load();
            segment->pins.fetch_add(kPin);
            if (root.load() == segment) return segment;
            release(segment);
        }
    }

    /// Drops one pin; a nonzero result is a retired segment whose last pin
    /// this was, now this thread's alone to recycle.
    static Segment* unpin(Segment* segment) noexcept {
        if (segment->pins.fetch_sub(kPin) != (kPin | kRetired)) return nullptr;
        std::uint64_t unpinned = kRetired;
        return segment->pins.compare_exchange_strong(unpinned, 0) ? segment : nullptr;
    }

    /// Drops one pin and recycles every segment that leaves this thread
    /// holding: reset, then linked as the newest. Linking pins the last
    /// segment, whose release may hand over another, so the segments to
    /// recycle are kept on a local list (through `next`, unused while a
    /// segment is out of the chain) instead of recursing.
    void release(Segment* held) noexcept {
        Segment* recycling = unpin(held);
        if (recycling != nullptr) recycling->next.store(nullptr, std::memory_order_relaxed);
        while (recycling != nullptr) {
            Segment* segment = recycling;
            recycling = segment->next.load(std::memory_order_relaxed);
            for (Entry& entry : segment->entries) entry.state.store(State::FREE, std::memory_order_relaxed);
            segment->written.store(0, std::memory_order_relaxed);
            segment->skip.store(0, std::memory_order_relaxed);
            segment->next.store(nullptr, std::memory_order_relaxed);
            for (bool is_linked = false; !is_linked;) {
                Segment* last = _last.load();
                last->pins.fetch_add(kPin);
                if (_last.load() == last) {
                    Segment* next = nullptr;
                    is_linked = last->next.compare_exchange_strong(next, segment);
                    advance(_last, *last, is_linked ? segment : next);
                }
                Segment* also = unpin(last);
                if (also == nullptr) continue;
                also->next.store(recycling, std::memory_order_relaxed);
                recycling = also;
            }
        }
    }

    /// Producer::push, on the segment `pinned` holds; pins the newest segment
    /// when it holds none or the one it holds is full.
    template <class U>
    void write(Segment*& pinned, U&& value) {
        for (;;) {
            if (pinned == nullptr) pinned = pin(_tail);
            Segment& segment = *pinned;
            // A thread adds to a full segment's count at most once before it
            // moves on, so the 64-bit count cannot run away.
            const std::uint64_t index = segment.written.fetch_add(1);
            if (index < Entries) {
                // The index is this thread's alone, and the segment cannot be
                // retired while one of its entries is unwritten.
                Entry& entry = segment.entries[static_cast<std::size_t>(index)];
                ::new (static_cast<void*>(entry.storage.data())) T(std::forward<U>(value));
                entry.state.store(State::PENDING);
                return;
            }
            // Full: move the queue on to the next segment, growing if there is none.
            Segment* next = segment.next.load();
            if (next == nullptr) {
                std::unique_ptr<Segment> grown = std::make_unique<Segment>();
                if (segment.next.compare_exchange_strong(next, grown.get())) {
                    next = grown.release();
                    _segments.fetch_add(1, std::memory_order_relaxed);
                }  // otherwise another producer grew it, and `grown` was never visible
            }
            advance(_last, segment, next);
            advance(_tail, segment, next);
            release(std::exchange(pinned, nullptr));
        }
    }

    /// Consumer::pop, on the segment `pinned` holds from `index` on; pins the
    /// oldest segment when it holds none or the one it holds is used up.
    [[nodiscard]] std::optional<T> take(Segment*& pinned, std::uint64_t& index) noexcept {
        for (;;) {
            if (pinned == nullptr) {
                pinned = pin(_head);
                index = pinned->skip.load();
            }
            Segment& segment = *pinned;
            for (; index < Entries; ++index) {
                Entry& entry = segment.entries[static_cast<std::size_t>(index)];
                // The compare-and-swap is the first touch of the entry, as in
                // FF_Queue: loading the state first brought its cache line in
                // shared and then again exclusive, and cut the throughput of
                // eight consumers to a third (measured 2026-11-12, M5 Pro).
                State state = State::PENDING;
                if (!entry.state.compare_exchange_strong(state, State::CLAIMED, std::memory_order_seq_cst,
                                                         std::memory_order_acquire)) {
                    switch (state) {
                        case State::FREE: return std::nullopt;  // not written yet
                        case State::CLAIMED: continue;          // another consumer's: move on
                        case State::PENDING: continue;          // unreachable: the exchange would have succeeded
                    }
                }
                ++index;
                std::optional<T> value(std::move(*entry.value()));
                std::destroy_at(entry.value());
                return value;
            }
            // Every entry is claimed: move the queue past this segment and
            // retire it. A cursor that stayed on a segment the queue has
            // already left fails each exchange and only lets go.
            Segment* next = segment.next.load();
            if (next == nullptr) return std::nullopt;  // the newest segment, used up; the next push grows
            advance(_last, segment, next);
            advance(_tail, segment, next);
            Segment* expected = &segment;
            if (_head.compare_exchange_strong(expected, next)) segment.pins.fetch_or(kRetired);
            release(std::exchange(pinned, nullptr));
        }
    }

    /// A consumer lets go of `segment`, having seen every entry below `index`
    /// claimed: later cursors start there instead of at the first entry.
    void leave(Segment* segment, const std::uint64_t index) noexcept {
        // Fails only when another consumer has raised it meanwhile.
        for (std::uint64_t skip = segment->skip.load(); skip < index;)
            if (segment->skip.compare_exchange_weak(skip, index)) break;
        release(segment);
    }

    /// Moves `root` from `from` to its successor, unless another thread has.
    /// Pre: `from` is pinned, so `root` naming it means it is not recycled.
    static void advance(std::atomic<Segment*>& root, Segment& from, Segment* to) noexcept {
        Segment* expected = &from;
        root.compare_exchange_strong(expected, to);
    }

    // _head <= _tail <= _last along the chain. A segment is retired only after
    // all three have left it, and each leaves it only forwards.
    alignas(kCacheLine) std::atomic<Segment*> _head;  // consumers claim here
    alignas(kCacheLine) std::atomic<Segment*> _tail;  // producers write here
    alignas(kCacheLine) std::atomic<Segment*> _last;  // recycled segments are linked after it
    std::atomic<std::size_t> _segments{0};
};

} // END NAMESPACE FIFO
} // END NAMESPACE IRIS
#endif /* IrisQueue_h */
