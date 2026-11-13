//
//  IrisAsync.hpp
//  Iris
//
//  Created by Ryan Landvater on 10/4/23.
//  Rebuilt 2026-11-12 (AsyncRebuilt_4.12, development/MODERNIZATION.md).
//

#ifndef IrisAsync_h
#define IrisAsync_h

// Self-contained: compiles with no other header first (StandaloneHeaderBuild_1.1).
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <IrisQueue.hpp>

#ifndef IRIS_CONCURRENCY
#define IRIS_CONCURRENCY std::thread::hardware_concurrency()
#endif

namespace Iris {
/**
 * @brief The Iris thread pool: workers that run tasks issued from any thread.
 *
 * - Work is distributed through one Iris::FIFO::Queue. Issuing a task is a
 *   push and a worker taking one is a pop, so neither waits for another
 *   thread: there is no mutex and no condition variable in the pool.
 * - A task is a move-only callable stored inline in its queue entry
 *   (kTaskStorage bytes), so issuing one allocates nothing.
 * - An idle worker sleeps on std::atomic::wait, after finding the queue
 *   empty twice. An issuer wakes one only when a worker has registered as
 *   asleep, which costs the issuer one atomic load when every worker is busy.
 * - wait_until_complete(), Fence::wait() and the destructor block their
 *   caller, on std::atomic::wait. They are for threads that may block; a
 *   thread that may not (a browser's main thread) polls pending_tasks() or
 *   Fence::outcome() instead.
 */
namespace Async {

/// Bytes a task's callable may occupy. With the fence and the entry's state,
/// a task then fills one 128-byte queue entry on a 64-bit target.
inline constexpr std::size_t kTaskStorage = 88;

/// A move-only callable taking and returning nothing, held in fixed inline
/// storage and run at most once. A callable that does not fit kTaskStorage, or
/// whose move can throw, does not build one (Iris-Core's Callback follows the
/// same rule): capture a pointer to large state instead of the state.
class Task {
    template <class F>
    static constexpr bool kStoredInline = sizeof(F) <= kTaskStorage && alignof(F) <= alignof(std::max_align_t) &&
                                          std::is_nothrow_move_constructible_v<F>;

public:
    Task() noexcept = default;

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, Task>) && kStoredInline<std::decay_t<F>> &&
                std::invocable<std::decay_t<F>&> && std::constructible_from<std::decay_t<F>, F>
    explicit Task(F&& body) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>) {
        ::new (static_cast<void*>(_storage.data())) std::decay_t<F>(std::forward<F>(body));
        _operations = &kOperations<std::decay_t<F>>;
    }

    Task(Task&& other) noexcept : _operations(std::exchange(other._operations, nullptr)) {
        if (_operations != nullptr) _operations->relocate(_storage.data(), other._storage.data());
    }
    Task& operator=(Task&& other) noexcept {
        if (this == &other) return *this;
        reset();
        _operations = std::exchange(other._operations, nullptr);
        if (_operations != nullptr) _operations->relocate(_storage.data(), other._storage.data());
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() { reset(); }

    [[nodiscard]] bool is_empty() const noexcept { return _operations == nullptr; }

    /// Runs the callable, destroys it (also when it throws) and leaves this
    /// empty. Pre: not empty.
    void operator()() && { std::exchange(_operations, nullptr)->run(_storage.data()); }

    /// Destroys the callable unrun.
    void reset() noexcept {
        if (_operations != nullptr) std::exchange(_operations, nullptr)->destroy(_storage.data());
    }

private:
    struct Operations {
        void (*run)(std::byte* storage);                             // runs, then destroys
        void (*relocate)(std::byte* to, std::byte* from) noexcept;   // moves, then destroys `from`
        void (*destroy)(std::byte* storage) noexcept;
    };
    template <class F>
    static F& stored(std::byte* storage) noexcept {
        return *std::launder(reinterpret_cast<F*>(storage));
    }
    template <class F>
    static constexpr Operations kOperations{
        .run =
            [](std::byte* storage) {
                struct DestroyAfter {
                    F& body;
                    ~DestroyAfter() { std::destroy_at(&body); }
                } destroy_after{stored<F>(storage)};
                std::invoke(destroy_after.body);
            },
        .relocate =
            [](std::byte* to, std::byte* from) noexcept {
                ::new (static_cast<void*>(to)) F(std::move(stored<F>(from)));
                std::destroy_at(&stored<F>(from));
            },
        .destroy = [](std::byte* storage) noexcept { std::destroy_at(&stored<F>(storage)); },
    };

    alignas(std::max_align_t) std::array<std::byte, kTaskStorage> _storage;
    const Operations* _operations = nullptr;
};

/// What a pool accepts: a callable taking no arguments that is nothrow-movable
/// and fits kTaskStorage bytes.
template <class F>
concept TaskBody = std::constructible_from<Task, F>;

/// A task's completion signal: settled exactly once, whatever becomes of the
/// task, so a waiter is always released. Copies share the signal.
class Fence {
public:
    enum class Outcome : std::uint8_t {
        PENDING,    ///< issued, not yet settled
        COMPLETE,   ///< the task ran
        FAILED,     ///< the task threw; the pool reported it and went on
        DISCARDED,  ///< the pool was terminated before the task ran
    };

    /// Never blocks.
    [[nodiscard]] Outcome outcome() const noexcept { return _signal->load(); }

    /// Blocks until the task is settled. Pre: not called from a task of the
    /// same pool with one worker, which would wait on itself.
    Outcome wait() const noexcept {
        Outcome settled = _signal->load();
        while (settled == Outcome::PENDING) {
            _signal->wait(Outcome::PENDING);
            settled = _signal->load();
        }
        return settled;
    }

private:
    friend class Pool;
    using Signal = std::atomic<Outcome>;
    Fence() noexcept = default;  // no signal: a task issued without a fence
    explicit Fence(std::shared_ptr<Signal> signal) noexcept : _signal(std::move(signal)) {}
    void settle(const Outcome outcome) const noexcept {
        if (_signal == nullptr) return;
        _signal->store(outcome);
        _signal->notify_all();
    }
    std::shared_ptr<Signal> _signal;
};

class Pool {
public:
    /// Starts `workers` threads (at least one).
    explicit Pool(std::uint32_t workers = IRIS_CONCURRENCY);
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;
    /// Runs every pending task, then stops the workers. Pre: not on a worker.
    ~Pool();

    /// Any thread, a worker included; never blocks. Returns false, with the
    /// callable destroyed unrun, when the pool has been terminated.
    template <TaskBody F>
    [[nodiscard]] bool issue_task(F&& body) {
        return publish(Job{Task(std::forward<F>(body)), Fence()});
    }

    /// As issue_task, with the task's completion signal. On a terminated pool
    /// the fence is already DISCARDED. Allocates the signal.
    template <TaskBody F>
    [[nodiscard]] Fence issue_task_with_fence(F&& body) {
        Fence fence(std::make_shared<Fence::Signal>(Fence::Outcome::PENDING));
        static_cast<void>(publish(Job{Task(std::forward<F>(body)), fence}));
        return fence;
    }

    /// Tasks issued and not yet finished: queued or running. Never blocks.
    [[nodiscard]] std::size_t pending_tasks() const noexcept {
        const std::uint64_t finished = _finished.load();  // read first, so the difference is never negative
        return static_cast<std::size_t>(_issued.load() - finished);
    }

    /// Blocks until no task is pending. The pool stays usable. Pre: not on a
    /// worker, whose own task would still be pending.
    void wait_until_complete() noexcept;

    /// Stops the workers after the task each is running, destroys the tasks
    /// not yet run (settling their fences DISCARDED) and refuses new ones.
    /// Pre: not on a worker.
    void terminate() noexcept;

private:
    enum class State : std::uint8_t { ACTIVE, TERMINATED };

    struct Job {
        Task task;
        Fence fence;
    };

    bool publish(Job&& job);
    void work() noexcept;
    void run(Job& job) noexcept;
    void finish(Job& job, Fence::Outcome outcome) noexcept;
    void count_finished() noexcept;
    void discard_unrun() noexcept;
    void wake_one() noexcept;

    FIFO::Queue<Job> _tasks;
    /// Tasks issued and tasks finished, counted apart so that issuers and
    /// workers each write their own cache line: one counter written by both
    /// held one issuer and one worker to 13.2 M trivial tasks/s, where the two
    /// reach 26.2 (measured 2026-11-12, M5 Pro).
    alignas(FIFO::kCacheLine) std::atomic<std::uint64_t> _issued{0};
    alignas(FIFO::kCacheLine) std::atomic<std::uint64_t> _finished{0};
    /// Threads in wait_until_complete.
    alignas(FIFO::kCacheLine) std::atomic<std::uint32_t> _waiters{0};
    /// Workers asleep, or deciding whether to sleep.
    alignas(FIFO::kCacheLine) std::atomic<std::uint32_t> _sleepers{0};
    /// Advanced to wake a sleeper. 64 bits, so a sleeper never meets the value
    /// it read again.
    std::atomic<std::uint64_t> _wake{0};
    std::atomic<State> _state{State::ACTIVE};
    std::vector<std::thread> _workers;
};

using ThreadPool = std::shared_ptr<Pool>;

/// A shared pool, for owners that hand it to several components.
[[nodiscard]] ThreadPool create_thread_pool(std::uint32_t workers = IRIS_CONCURRENCY);

} // END ASYNC NAMESPACE
} // END IRIS NAMESPACE
#endif /* IrisAsync_h */
