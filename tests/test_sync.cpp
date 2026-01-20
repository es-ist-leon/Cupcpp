// Tests for async synchronization primitives

#include <cupcpp/cupcpp.hpp>
#include <vector>
#include <string>

// Test framework declarations
namespace test {
    int register_test(const std::string& name, std::function<void()> func);
    void check(bool condition, const std::string& message, const char* file, int line);
}

#define TEST(name) \
    void test_##name(); \
    static int test_##name##_registered = test::register_test(#name, test_##name); \
    void test_##name()

#define CHECK(cond) test::check(cond, #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) test::check((a) == (b), std::string(#a " == " #b), __FILE__, __LINE__)

// Tests for AsyncMutex

TEST(mutex_try_lock) {
    cupcpp::AsyncMutex mutex;

    CHECK(mutex.try_lock());
    CHECK(!mutex.try_lock());  // Already locked

    mutex.unlock();

    CHECK(mutex.try_lock());  // Can lock again
    mutex.unlock();
}

TEST(mutex_scoped_lock) {
    cupcpp::AsyncMutex mutex;

    {
        auto lock = mutex.try_lock();
        CHECK(lock);
        CHECK(!mutex.try_lock());  // Locked
    }

    // Lock released
    CHECK(mutex.try_lock());
    mutex.unlock();
}

// Tests for AsyncSemaphore

TEST(semaphore_basic) {
    cupcpp::AsyncSemaphore sem(3);

    CHECK_EQ(sem.available(), 3u);

    CHECK(sem.try_acquire());
    CHECK_EQ(sem.available(), 2u);

    CHECK(sem.try_acquire());
    CHECK(sem.try_acquire());
    CHECK_EQ(sem.available(), 0u);

    CHECK(!sem.try_acquire());  // No more available

    sem.release();
    CHECK_EQ(sem.available(), 1u);

    CHECK(sem.try_acquire());
    CHECK_EQ(sem.available(), 0u);
}

TEST(semaphore_release_multiple) {
    cupcpp::AsyncSemaphore sem(0);

    CHECK_EQ(sem.available(), 0u);

    sem.release(5);
    CHECK_EQ(sem.available(), 5u);
}

// Tests for AsyncEvent

TEST(event_basic) {
    cupcpp::AsyncEvent event;

    CHECK(!event.is_set());

    event.set();
    CHECK(event.is_set());

    event.reset();
    CHECK(!event.is_set());

    event.set();
    CHECK(event.is_set());
}

// Tests for AsyncLatch

TEST(latch_basic) {
    cupcpp::AsyncLatch latch(3);

    CHECK(!latch.try_wait());

    latch.count_down();
    CHECK(!latch.try_wait());

    latch.count_down();
    CHECK(!latch.try_wait());

    latch.count_down();
    CHECK(latch.try_wait());
}

TEST(latch_count_down_multiple) {
    cupcpp::AsyncLatch latch(5);

    CHECK(!latch.try_wait());

    latch.count_down(3);
    CHECK(!latch.try_wait());

    latch.count_down(2);
    CHECK(latch.try_wait());
}

TEST(latch_overcount) {
    cupcpp::AsyncLatch latch(2);

    latch.count_down(10);  // More than needed
    CHECK(latch.try_wait());
}

// Tests for Channel

TEST(channel_buffered_basic) {
    cupcpp::Channel<int> ch(3);

    CHECK(ch.try_send(1));
    CHECK(ch.try_send(2));
    CHECK(ch.try_send(3));
    CHECK(!ch.try_send(4));  // Buffer full

    CHECK_EQ(ch.size(), 3u);

    auto v1 = ch.try_receive();
    CHECK(v1.has_value());
    CHECK_EQ(*v1, 1);

    auto v2 = ch.try_receive();
    CHECK(v2.has_value());
    CHECK_EQ(*v2, 2);

    auto v3 = ch.try_receive();
    CHECK(v3.has_value());
    CHECK_EQ(*v3, 3);

    auto v4 = ch.try_receive();
    CHECK(!v4.has_value());  // Empty
}

TEST(channel_close) {
    cupcpp::Channel<int> ch(5);

    CHECK(!ch.is_closed());

    ch.try_send(1);
    ch.try_send(2);

    ch.close();
    CHECK(ch.is_closed());

    CHECK(!ch.try_send(3));  // Can't send to closed

    // Can still receive buffered values
    auto v1 = ch.try_receive();
    CHECK(v1.has_value());
    CHECK_EQ(*v1, 1);

    auto v2 = ch.try_receive();
    CHECK(v2.has_value());
    CHECK_EQ(*v2, 2);
}

TEST(channel_empty_check) {
    cupcpp::Channel<int> ch(5);

    CHECK(ch.empty());

    ch.try_send(42);
    CHECK(!ch.empty());

    ch.try_receive();
    CHECK(ch.empty());
}
