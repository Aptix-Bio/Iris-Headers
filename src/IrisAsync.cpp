//
//  IrisAsync.cpp
//  Iris
//
//  Created by Ryan Landvater on 10/4/23.
//  Rebuilt 2026-11-12 (AsyncRebuilt_4.12, development/MODERNIZATION.md).
//
#include "IrisAsync.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <iostream>
#include <new>
#include <sstream>
#include <string_view>

namespace Iris {
namespace Async {
namespace {
/// A thread is a boundary no exception crosses: the failure is reported with
/// its message and the pool goes on.
void report_task_failure(const std::string_view what) noexcept {
    try {
        std::stringstream log;
        log << "[WARNING] Iris Async Pool: a task threw: " << what << "\n";
        std::cerr << log.str();
    } catch (const std::exception&) {
        // Reporting failed (out of memory, or the stream throws): the task's
        // fence still settles FAILED.
    }
}
}  // namespace

ThreadPool create_thread_pool(const std::uint32_t workers) { return std::make_shared<Pool>(workers); }

Pool::Pool(const std::uint32_t workers) {
    const std::uint32_t count = std::max<std::uint32_t>(workers, 1);
    _workers.reserve(count);
    for (std::uint32_t worker = 0; worker < count; ++worker) _workers.emplace_back([this] { work(); });
}

Pool::~Pool() {
    wait_until_complete();
    terminate();
}

// Every atomic operation below is sequentially consistent, as is the queue's
// store that publishes an entry; a pop's reads are ordered after them by the
// fence before it. Two handshakes rest on that order (C++20 [atomics.order]
// p4: https://timsong-cpp.github.io/cppwp/n4861/atomics.order#4, read
// 2026-11-12):
// - Sleep: a worker registers in _sleepers, reads _wake, then pops; an issuer
//   pushes, then reads _sleepers and, finding one, advances _wake. Whichever
//   comes second in the order sees the other: the worker finds the task, or
//   the issuer finds the sleeper and the worker's wait on the value of _wake
//   it read returns. So a task is never left queued with every worker asleep,
//   and a worker never sleeps holding a task.
// - Terminate: an issuer pushes, then reads _state; terminate() stores _state,
//   then discards. So a task pushed across a terminate is discarded by one of
//   them and never stranded.
bool Pool::publish(Job&& job) {
    switch (_state.load()) {
        case State::ACTIVE: break;
        case State::TERMINATED:
            job.task.reset();
            job.fence.settle(Fence::Outcome::DISCARDED);
            return false;
    }
    // Counted before it is pushed: a worker may finish it before push returns.
    _issued.fetch_add(1);
    try {
        _tasks.push(std::move(job));
    } catch (const std::bad_alloc&) {
        // The queue could not grow and left the job untouched; it is not pending.
        count_finished();
        throw;
    }
    wake_one();
    if (_state.load() == State::TERMINATED) discard_unrun();
    return true;
}

void Pool::work() noexcept {
    FIFO::Queue<Job>::Consumer tasks(_tasks);
    for (;;) {
        switch (_state.load()) {
            case State::ACTIVE: break;
            case State::TERMINATED: return;
        }
        std::optional<Job> job = tasks.pop();
        if (!job.has_value()) {
            // Nothing to claim. Register as asleep, then look once more.
            _sleepers.fetch_add(1);
            const std::uint64_t wake = _wake.load();
            std::atomic_thread_fence(std::memory_order_seq_cst);
            job = tasks.pop();
            if (!job.has_value() && _state.load() == State::ACTIVE) {
                tasks.release();  // a sleeping worker pins no segment
                _wake.wait(wake);
            }
            _sleepers.fetch_sub(1);
            if (!job.has_value()) continue;
        }
        run(*job);
    }
}

void Pool::run(Job& job) noexcept {
    Fence::Outcome outcome = Fence::Outcome::COMPLETE;
    try {
        std::move(job.task)();
    } catch (const std::exception& error) {
        report_task_failure(error.what());
        outcome = Fence::Outcome::FAILED;
    } catch (...) {
        report_task_failure("an exception not derived from std::exception");
        outcome = Fence::Outcome::FAILED;
    }
    finish(job, outcome);
}

void Pool::finish(Job& job, const Fence::Outcome outcome) noexcept {
    // The callable's captures are released before anyone waiting is told.
    job.task.reset();
    job.fence.settle(outcome);
    count_finished();
}

void Pool::count_finished() noexcept {
    // Workers write _finished and issuers write _issued, so neither side takes
    // the other's cache line on every task. A thread in wait_until_complete is
    // notified by the finish that brings the two level, and only when one has
    // registered: the waiter registers, then reads _finished; this adds, then
    // reads _waiters; so one of them sees the other.
    const std::uint64_t finished = _finished.fetch_add(1) + 1;
    if (_waiters.load() != 0 && _issued.load() == finished) _finished.notify_all();
}

void Pool::discard_unrun() noexcept {
    FIFO::Queue<Job>::Consumer tasks(_tasks);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (std::optional<Job> job = tasks.pop(); job.has_value(); job = tasks.pop())
        finish(*job, Fence::Outcome::DISCARDED);
}

void Pool::wake_one() noexcept {
    if (_sleepers.load() == 0) return;  // every worker is busy: the issuer's whole cost here
    _wake.fetch_add(1);
    _wake.notify_one();
}

void Pool::wait_until_complete() noexcept {
    _waiters.fetch_add(1);
    // A finish that leaves tasks pending changes _finished without a notify;
    // the waiter then sleeps on until the finish that levels the counts.
    for (std::uint64_t finished = _finished.load(); _issued.load() != finished; finished = _finished.load())
        _finished.wait(finished);
    _waiters.fetch_sub(1);
}

void Pool::terminate() noexcept {
    switch (_state.exchange(State::TERMINATED)) {
        case State::ACTIVE:
            _wake.fetch_add(1);
            _wake.notify_all();
            for (std::thread& worker : _workers) worker.join();
            break;
        case State::TERMINATED: break;  // an earlier call stopped the workers
    }
    discard_unrun();
}

} // END ASYNC NAMESPACE
} // END IRIS NAMESPACE
