// TypeTests_1.3 (development/MODERNIZATION.md, SafetyNet_1): today's behaviour
// of the types consumers rely on, recorded before Modernize_4 rewrites them, so
// each change there is a visible, deliberate edit to an expectation here.
//
// Not tested, because they cannot be called: IrisCore.hpp declares
// Buffer_change_strength, which nothing defines, and Buffer_write_into_buffer
// with a signature src/IrisBuffer.cpp does not define (it defines a
// (Buffer&, size_t&) overload). Both are fixed in RuntimeLibraries_4.5.
#include <IrisCodecTypes.hpp>
#include <IrisCore.hpp>
#include <IrisAsync.hpp>
#include <IrisBuffer.hpp>
#include <IrisMemory.hpp>
#include <IrisQueue.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <thread>
#include <vector>

// ResultRedesigned_4.1 (ResultShapeQuestion_3, Ryan 2026-11-12): a scoped
// severity, the project the error belongs to, that project's own code, a
// message carrying the offending value, and where it was made; checked by name,
// with no conversion to bool.
namespace {
enum class ProbeError : std::uint32_t { BAD_ENCODING = 7 };
}

TEST_CASE("Result: a default Result fails safe", "[Result]") {
    const Iris::Result unassigned;
    CHECK(unassigned.failed());
    CHECK_FALSE(unassigned.succeeded());
    CHECK(unassigned.severity == Iris::Severity::FAILURE);
}

TEST_CASE("Result: success, warning and failure carry their domain and code", "[Result]") {
    const Iris::Result ok = Iris::Result::success();
    CHECK(ok.succeeded());
    CHECK_FALSE(ok.is_warning());

    const Iris::Result warned = Iris::Result::warning(Iris::Domain::IRIS_CORE, ProbeError::BAD_ENCODING, "slow path");
    CHECK(warned.succeeded());
    CHECK(warned.is_warning());
    CHECK(warned.domain == Iris::Domain::IRIS_CORE);
    CHECK(warned.code == 7);

    const Iris::Result failed =
        Iris::Result::failure(Iris::Domain::IRIS_CODEC, ProbeError::BAD_ENCODING, "unknown encoding: 7");
    CHECK(failed.failed());
    CHECK_FALSE(failed.is_warning());
    CHECK(failed.message == "unknown encoding: 7");
    CHECK(failed.code_as<ProbeError>() == ProbeError::BAD_ENCODING);
}

TEST_CASE("Result: records where it was made", "[Result]") {
    const std::uint_least32_t line = std::source_location::current().line();
    const Iris::Result failed = Iris::Result::failure(Iris::Domain::IRIS_CORE, ProbeError::BAD_ENCODING, "x");
    CHECK(failed.where.line() == line + 1);
    CHECK(std::string_view(failed.where.file_name()).ends_with("types_test.cpp"));
}

TEST_CASE("Result: is checked by name, never converted to bool", "[Result]") {
    STATIC_REQUIRE_FALSE(std::is_convertible_v<Iris::Result, bool>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<bool, Iris::Result>);
    STATIC_REQUIRE(noexcept(std::declval<const Iris::Result&>().succeeded()));
    STATIC_REQUIRE(noexcept(std::declval<const Iris::Result&>().failed()));
    STATIC_REQUIRE(std::is_nothrow_default_constructible_v<Iris::Result>);
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<Iris::Result>);
}

TEST_CASE("Result: Iris-Headers' own failures name their domain and code", "[Result][Memory]") {
    Iris::Memory arena;
    Iris::MemoryCreateInfo info;
    info.capacity = 0;
    const Iris::Result refused = Iris::create_memory(info, arena);
    CHECK(refused.failed());
    CHECK(refused.domain == Iris::Domain::IRIS_HEADERS);
    CHECK(refused.code_as<Iris::HeadersError>() == Iris::HeadersError::MEMORY_INVALID_ARGUMENT);
    CHECK(refused.message.find("capacity is zero") != std::string::npos);
}

TEST_CASE("Result: the codec's failures carry its own domain and code", "[Result]") {
    static_assert(Iris::ResultCode<IrisCodec::CodecError>);
    const Iris::Result result = Iris::Result::failure(Iris::Domain::IRIS_CODEC,
                                                      IrisCodec::CodecError::FILE_NOT_OPENED, "missing.iris");
    CHECK(result.failed());
    CHECK(result.domain == Iris::Domain::IRIS_CODEC);
    CHECK(result.code_as<IrisCodec::CodecError>() == IrisCodec::CodecError::FILE_NOT_OPENED);
}

TEST_CASE("Buffer: a strong copy owns its bytes; a weak wrap shares them", "[Buffer]") {
    std::array<char, 4> source{'a', 'b', 'c', 'd'};
    const Iris::Buffer strong = Iris::Copy_strong_buffer_from_data(source.data(), source.size());
    const Iris::Buffer weak = Iris::Wrap_weak_buffer_fom_data(source.data(), source.size());
    source[0] = 'z';

    void* data = nullptr;
    size_t bytes = 0;
    REQUIRE(Iris::Buffer_get_data(strong, data, bytes).succeeded());
    CHECK(bytes == 4);
    CHECK(std::memcmp(data, "abcd", 4) == 0);

    REQUIRE(Iris::Buffer_get_data(weak, data, bytes).succeeded());
    CHECK(data == source.data());
    CHECK(static_cast<const char*>(data)[0] == 'z');
}

// Known defect, found 2026-11-12: append(data, size) grows the capacity, then
// fails because growing the capacity leaves the size unchanged, so an append
// that needs room writes nothing (16 bytes into a 4-byte buffer: size stays 0).
// Expected to fail ([!shouldfail]) until BufferAppendGrows_4.9 fixes it.
TEST_CASE("Buffer: appending past a strong buffer's capacity grows it", "[Buffer][!shouldfail]") {
    const Iris::Buffer buffer = Iris::Create_strong_buffer(4);
    std::array<char, 16> bytes{};
    bytes.fill('q');
    const Iris::Result appended = buffer->append(bytes.data(), bytes.size());
    CHECK(appended.succeeded());
    CHECK(buffer->size() == bytes.size());
}

TEST_CASE("Buffer: a strong buffer created with a size reports it empty", "[Buffer]") {
    const Iris::Buffer buffer = Iris::Create_strong_buffer(64);
    void* data = nullptr;
    size_t bytes = 1;
    REQUIRE(Iris::Buffer_get_data(buffer, data, bytes).succeeded());
    CHECK(bytes == 0);
}

// AsyncRebuilt_4.12 (development/MODERNIZATION.md): the Iris FIFO holds entries
// by value in fixed segments; a consumer claims an entry by compare-and-swap on
// the entry's own state and moves on when another consumer has it. The pool
// distributes work through that queue with no mutex.
namespace {
/// Counts live instances, so a test can show nothing was leaked or run twice.
struct Counted {
    static inline std::atomic<long> alive{0};
    long value = 0;
    explicit Counted(const long v) noexcept : value(v) { ++alive; }
    Counted(Counted&& other) noexcept : value(other.value) { ++alive; }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted& operator=(Counted&&) = delete;
    ~Counted() { --alive; }
};
/// Entries per segment in the stress tests: small, so segments are exhausted
/// and recycled thousands of times in a short run.
constexpr std::size_t kSmallSegment = 8;
}  // namespace

TEST_CASE("FIFO queue: first in, first out; popping an empty queue returns nothing", "[Queue]") {
    Iris::FIFO::Queue<long, 4> queue;
    Iris::FIFO::Queue<long, 4>::Consumer consumer(queue);
    CHECK_FALSE(consumer.pop().has_value());
    for (long value = 1; value <= 10; ++value) queue.push(value);  // crosses two segment boundaries
    for (long expected = 1; expected <= 10; ++expected) {
        const std::optional<long> popped = consumer.pop();
        REQUIRE(popped.has_value());
        CHECK(*popped == expected);
    }
    CHECK_FALSE(consumer.pop().has_value());
    queue.push(11);  // the queue is reusable after it empties
    CHECK(consumer.pop() == std::optional<long>{11});
    Iris::FIFO::Queue<long, 4>::Consumer late(queue);  // a cursor made late starts at the oldest unclaimed entry
    queue.push(12);
    CHECK(late.pop() == std::optional<long>{12});
}

TEST_CASE("FIFO queue: holds move-only entries, moved in and out", "[Queue]") {
    Iris::FIFO::Queue<std::unique_ptr<int>> queue;
    queue.push(std::make_unique<int>(7));
    Iris::FIFO::Queue<std::unique_ptr<int>>::Consumer consumer(queue);
    std::optional<std::unique_ptr<int>> popped = consumer.pop();
    REQUIRE(popped.has_value());
    CHECK(**popped == 7);
}

TEST_CASE("FIFO queue: exhausted segments are recycled, so a steady flow allocates nothing", "[Queue]") {
    Iris::FIFO::Queue<long, kSmallSegment> queue;
    Iris::FIFO::Queue<long, kSmallSegment>::Producer producer(queue);
    Iris::FIFO::Queue<long, kSmallSegment>::Consumer consumer(queue);
    for (long value = 0; value < 100000; ++value) {
        producer.push(value);
        REQUIRE(consumer.pop() == std::optional<long>{value});
    }
    CHECK(queue.segments() <= 3);  // 12500 segments' worth of entries passed through
}

TEST_CASE("FIFO queue: entries still queued are destroyed with the queue", "[Queue]") {
    {
        Iris::FIFO::Queue<Counted, kSmallSegment> queue;
        for (long value = 0; value < 50; ++value) queue.push(Counted{value});
        Iris::FIFO::Queue<Counted, kSmallSegment>::Consumer consumer(queue);
        for (int taken = 0; taken < 20; ++taken) REQUIRE(consumer.pop().has_value());
        CHECK(Counted::alive.load() == 30);
    }
    CHECK(Counted::alive.load() == 0);
}

TEST_CASE("FIFO queue: several producers lose nothing and keep each producer's order", "[Queue]") {
    // QueueManyProducers_4.11: FIFO2 aborted under this load ("No valid tail").
    constexpr int kProducers = 8;
    constexpr long kEach = 100000;
    Iris::FIFO::Queue<long, kSmallSegment> queue;
    std::atomic<bool> go{false};
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p)
        producers.emplace_back([&queue, &go, p] {
            Iris::FIFO::Queue<long, kSmallSegment>::Producer cursor(queue);
            while (!go.load()) {}
            for (long i = 0; i < kEach; ++i) {
                if (p % 2 == 0) cursor.push(p * kEach + i);  // half through a cursor,
                else queue.push(p * kEach + i);              // half pinning per push
            }
        });
    go = true;
    Iris::FIFO::Queue<long, kSmallSegment>::Consumer consumer(queue);
    std::vector<long> last(kProducers, -1);
    long popped = 0, out_of_order = 0;
    while (popped < kProducers * kEach) {
        const std::optional<long> item = consumer.pop();
        if (!item.has_value()) continue;  // empty, or a producer has claimed an entry and not yet written it
        const auto producer = static_cast<std::size_t>(*item / kEach);
        if (*item % kEach <= last[producer]) ++out_of_order;
        last[producer] = *item % kEach;
        ++popped;
    }
    for (std::thread& producer : producers) producer.join();
    CHECK(out_of_order == 0);
    CHECK_FALSE(consumer.pop().has_value());
}

TEST_CASE("FIFO queue: several producers and several consumers take every entry exactly once", "[Queue]") {
    constexpr int kProducers = 6;
    constexpr int kConsumers = 6;
    constexpr long kEach = 50000;
    for (const std::size_t reserve : {std::size_t{0}, std::size_t{4096}}) {
        Iris::FIFO::Queue<Counted, kSmallSegment> queue(reserve);
        std::vector<std::atomic<std::uint8_t>> taken(static_cast<std::size_t>(kProducers * kEach));
        std::atomic<long> total{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> threads;
        for (int p = 0; p < kProducers; ++p)
            threads.emplace_back([&queue, &go, p] {
                while (!go.load()) {}
                for (long i = 0; i < kEach; ++i) queue.push(Counted{p * kEach + i});
            });
        for (int c = 0; c < kConsumers; ++c)
            threads.emplace_back([&queue, &go, &taken, &total] {
                while (!go.load()) {}
                while (total.load() < kProducers * kEach) {
                    // A cursor per pop: pinned, used once and let go, as a thread with no cursor of its own would.
                    const std::optional<Counted> item = Iris::FIFO::Queue<Counted, kSmallSegment>::Consumer(queue).pop();
                    if (!item.has_value()) continue;
                    taken[static_cast<std::size_t>(item->value)].fetch_add(1);
                    total.fetch_add(1);
                }
            });
        go = true;
        for (std::thread& thread : threads) thread.join();
        long wrong = 0;
        for (const std::atomic<std::uint8_t>& count : taken) wrong += count.load() == 1 ? 0 : 1;
        CHECK(wrong == 0);
        CHECK_FALSE(Iris::FIFO::Queue<Counted, kSmallSegment>::Consumer(queue).pop().has_value());
    }
    CHECK(Counted::alive.load() == 0);
}

TEST_CASE("FIFO queue: consumers with borrowed cursors take every entry exactly once", "[Queue]") {
    // A cursor keeps its segment pinned between operations; an idle cursor
    // holds back one segment and no other thread.
    constexpr int kProducers = 6;
    constexpr int kConsumers = 6;
    constexpr long kEach = 50000;
    {
        Iris::FIFO::Queue<Counted, kSmallSegment> queue;
        Iris::FIFO::Queue<Counted, kSmallSegment>::Consumer idle(queue);  // pinned for the whole run, never popping
        CHECK_FALSE(idle.pop().has_value());
        std::vector<std::atomic<std::uint8_t>> taken(static_cast<std::size_t>(kProducers * kEach));
        std::atomic<long> total{0};
        std::vector<std::thread> threads;
        for (int p = 0; p < kProducers; ++p)
            threads.emplace_back([&queue, p] {
                Iris::FIFO::Queue<Counted, kSmallSegment>::Producer cursor(queue);
                for (long i = 0; i < kEach; ++i) cursor.push(Counted{p * kEach + i});
            });
        for (int c = 0; c < kConsumers; ++c)
            threads.emplace_back([&queue, &taken, &total] {
                Iris::FIFO::Queue<Counted, kSmallSegment>::Consumer cursor(queue);
                while (total.load() < kProducers * kEach) {
                    const std::optional<Counted> item = cursor.pop();
                    if (!item.has_value()) continue;
                    taken[static_cast<std::size_t>(item->value)].fetch_add(1);
                    total.fetch_add(1);
                }
            });
        for (std::thread& thread : threads) thread.join();
        long wrong = 0;
        for (const std::atomic<std::uint8_t>& count : taken) wrong += count.load() == 1 ? 0 : 1;
        CHECK(wrong == 0);
        CHECK_FALSE(idle.pop().has_value());
        queue.push(Counted{-1});
        CHECK(idle.pop().has_value());  // the idle cursor catches up with the queue
    }
    CHECK(Counted::alive.load() == 0);
}

TEST_CASE("Async task: a callable is stored inline or does not build one", "[Async]") {
    struct Large {
        std::array<std::byte, Iris::Async::kTaskStorage + 1> bytes;
        void operator()() const noexcept {}
    };
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Iris::Async::Task, Large>);
    STATIC_REQUIRE(std::is_constructible_v<Iris::Async::Task, std::function<void()>>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Iris::Async::Task>);
    int ran = 0;
    Iris::Async::Task task{[&ran, owned = std::make_unique<int>(3)] { ran += *owned; }};  // move-only capture
    Iris::Async::Task moved = std::move(task);
    CHECK(task.is_empty());
    std::move(moved)();
    CHECK(ran == 3);
    CHECK(moved.is_empty());
}

TEST_CASE("Async pool: every issued task runs before wait_until_complete returns", "[Async]") {
    Iris::Async::Pool pool(4);
    std::atomic<int> ran{0};
    for (int round = 1; round <= 3; ++round) {  // the pool stays usable after each wait
        for (int task = 0; task < 100; ++task) REQUIRE(pool.issue_task([&ran] { ++ran; }));
        pool.wait_until_complete();
        CHECK(ran.load() == 100 * round);
        CHECK(pool.pending_tasks() == 0);
    }
}

// Found by CI on Windows (2026-11-12): a worker that had not reached its loop
// when the pool began draining never ran anything. Many short rounds make a
// start-up race show on any platform; the pool's destructor drains.
TEST_CASE("Async pool: a pool destroyed straight after issuing still runs every task", "[Async]") {
    for (int round = 0; round < 300; ++round) {
        std::atomic<int> ran{0};
        {
            const Iris::Async::ThreadPool pool = Iris::Async::create_thread_pool(4);
            for (int task = 0; task < 20; ++task) REQUIRE(pool->issue_task([&ran] { ++ran; }));
        }
        REQUIRE(ran.load() == 20);
    }
}

TEST_CASE("Async pool: tasks issued from several threads run exactly once each", "[Async]") {
    constexpr int kIssuers = 8;
    constexpr int kEach = 20000;
    std::vector<std::atomic<std::uint8_t>> runs(static_cast<std::size_t>(kIssuers * kEach));
    {
        Iris::Async::Pool pool(8);
        std::vector<std::thread> issuers;
        for (int t = 0; t < kIssuers; ++t)
            issuers.emplace_back([&pool, &runs, t] {
                for (int i = 0; i < kEach; ++i) {
                    std::atomic<std::uint8_t>* slot = &runs[static_cast<std::size_t>(t * kEach + i)];
                    if (!pool.issue_task([slot, held = Counted{i}] { slot->fetch_add(1); })) std::abort();
                }
            });
        for (std::thread& issuer : issuers) issuer.join();
        pool.wait_until_complete();
        long wrong = 0;
        for (const std::atomic<std::uint8_t>& count : runs) wrong += count.load() == 1 ? 0 : 1;
        CHECK(wrong == 0);
        CHECK(pool.pending_tasks() == 0);
        CHECK(Counted::alive.load() == 0);  // every task's captures were destroyed once it ran
    }
}

TEST_CASE("Async pool: pending_tasks counts what is queued or running, from any thread", "[Async]") {
    Iris::Async::Pool pool(2);
    std::atomic<bool> gate{false};
    std::atomic<int> started{0};
    for (int task = 0; task < 2; ++task)
        REQUIRE(pool.issue_task([&gate, &started] {
            ++started;
            while (!gate.load()) std::this_thread::yield();
        }));
    while (started.load() < 2) std::this_thread::yield();
    for (int task = 0; task < 5; ++task) REQUIRE(pool.issue_task([] {}));
    CHECK(pool.pending_tasks() == 7);  // two running, five queued behind them
    gate = true;
    pool.wait_until_complete();
    CHECK(pool.pending_tasks() == 0);
}

TEST_CASE("Async pool: a task may issue further tasks, and the wait covers them", "[Async]") {
    Iris::Async::Pool pool(4);
    std::atomic<int> ran{0};
    Iris::Async::Pool* issuer = &pool;
    for (int task = 0; task < 1000; ++task)
        REQUIRE(pool.issue_task([issuer, &ran] {
            ++ran;
            if (!issuer->issue_task([&ran] { ++ran; })) std::abort();
        }));
    pool.wait_until_complete();
    CHECK(ran.load() == 2000);
}

TEST_CASE("Async pool: a fence reports its task's outcome", "[Async]") {
    using Outcome = Iris::Async::Fence::Outcome;
    Iris::Async::Pool pool(2);
    std::atomic<bool> ran{false};
    const Iris::Async::Fence fence = pool.issue_task_with_fence([&ran] { ran = true; });
    CHECK(fence.wait() == Outcome::COMPLETE);
    CHECK(ran.load());
    CHECK(fence.outcome() == Outcome::COMPLETE);

    const Iris::Async::Fence failed = pool.issue_task_with_fence([] { throw std::runtime_error("probe"); });
    CHECK(failed.wait() == Outcome::FAILED);
    pool.wait_until_complete();  // the pool survives a task that throws
    CHECK(pool.pending_tasks() == 0);
}

TEST_CASE("Async pool: terminate frees the tasks it did not run and settles their fences", "[Async]") {
    using Outcome = Iris::Async::Fence::Outcome;
    constexpr int kWorkers = 2;
    constexpr int kQueued = 1000;
    Iris::Async::Pool pool(kWorkers);
    std::atomic<bool> gate{false};
    std::atomic<int> blocked{0};
    for (int worker = 0; worker < kWorkers; ++worker)
        REQUIRE(pool.issue_task([&gate, &blocked] {
            ++blocked;
            while (!gate.load()) std::this_thread::yield();
        }));
    while (blocked.load() < kWorkers) std::this_thread::yield();  // every worker is now busy

    std::atomic<int> ran{0};
    const std::shared_ptr<int> token = std::make_shared<int>(0);
    std::vector<Iris::Async::Fence> fences;
    for (int task = 0; task < kQueued; ++task) {
        REQUIRE(pool.issue_task([&ran, token] { ++ran; }));
        fences.push_back(pool.issue_task_with_fence([&ran, token] { ++ran; }));
    }
    CHECK(token.use_count() == 1 + 2 * kQueued);

    std::thread terminator([&pool] { pool.terminate(); });
    while (pool.issue_task([] {})) std::this_thread::yield();  // refused once the pool is terminated
    gate = true;
    terminator.join();

    CHECK(ran.load() == 0);
    CHECK(token.use_count() == 1);  // every unrun task's captures were destroyed
    CHECK(pool.pending_tasks() == 0);
    long undiscarded = 0;
    for (const Iris::Async::Fence& fence : fences) undiscarded += fence.wait() == Outcome::DISCARDED ? 0 : 1;
    CHECK(undiscarded == 0);
    CHECK(pool.issue_task_with_fence([] {}).wait() == Outcome::DISCARDED);
    pool.wait_until_complete();  // returns at once: nothing is pending
}

TEST_CASE("MemoryArena: an anonymous arena reserves its capacity and commits writable pages", "[Memory]") {
    Iris::Memory arena;
    Iris::MemoryCreateInfo info;
    info.capacity = 1u << 20;
    REQUIRE(Iris::create_memory(info, arena).succeeded());
    REQUIRE(static_cast<bool>(arena));
    CHECK(arena.capacity() >= info.capacity);
    arena.commit(0, 4096);
    arena.base()[0] = 0x5A;
    arena.base()[4095] = 0xA5;
    CHECK(arena.base()[0] == 0x5A);
    CHECK(arena.base()[4095] == 0xA5);
}
