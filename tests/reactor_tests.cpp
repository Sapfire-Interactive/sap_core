#include "sap_core/io/reactor.h"

#include <gtest/gtest.h>

#include <sys/eventfd.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <thread>

using sap::io::Event;
using sap::io::Reactor;
using sap::io::Trigger;

namespace {

    int make_eventfd(unsigned initial = 0) {
        int fd = ::eventfd(initial, EFD_CLOEXEC | EFD_NONBLOCK);
        return fd;
    }

    void write_eventfd(int fd, uint64_t v = 1) {
        ssize_t n = ::write(fd, &v, sizeof(v));
        (void)n;
    }

    void drain_eventfd(int fd) {
        uint64_t v;
        (void)::read(fd, &v, sizeof(v));
    }

} // namespace

TEST(ReactorTest, Create) {
    auto r = Reactor::create();
    ASSERT_TRUE(r.has_value()) << r.error();
}

TEST(ReactorTest, EmptyWaitReturnsZero) {
    auto r = Reactor::create();
    ASSERT_TRUE(r.has_value());
    Trigger out[8];
    auto n = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(10));
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 0u);
}

TEST(ReactorTest, ZeroTimeoutPolls) {
    auto r = Reactor::create();
    ASSERT_TRUE(r.has_value());
    Trigger out[8];
    auto start = std::chrono::steady_clock::now();
    auto n     = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(0));
    auto dt    = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 0u);
    EXPECT_LT(dt, std::chrono::milliseconds(50));
}

TEST(ReactorTest, ReadableEventfdTriggers) {
    auto r = Reactor::create();
    int  fd = make_eventfd();
    ASSERT_GE(fd, 0);

    constexpr u64 token = 0xCAFE;
    ASSERT_TRUE(r.value().add(fd, Event::Readable, token).has_value());

    write_eventfd(fd);

    Trigger out[8];
    auto    n = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(500));
    ASSERT_TRUE(n.has_value());
    ASSERT_EQ(n.value(), 1u);
    EXPECT_EQ(out[0].user_token, token);
    EXPECT_TRUE(sap::io::has(out[0].events, Event::Readable));

    EXPECT_TRUE(r.value().remove(fd).has_value());
    ::close(fd);
}

TEST(ReactorTest, MultipleFdsMultiplex) {
    auto r   = Reactor::create();
    int  fd1 = make_eventfd();
    int  fd2 = make_eventfd();
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);

    ASSERT_TRUE(r.value().add(fd1, Event::Readable, 111).has_value());
    ASSERT_TRUE(r.value().add(fd2, Event::Readable, 222).has_value());

    write_eventfd(fd1);
    write_eventfd(fd2);

    Trigger out[8];
    auto    n = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(500));
    ASSERT_TRUE(n.has_value());
    ASSERT_EQ(n.value(), 2u);

    bool saw_111 = false, saw_222 = false;
    for (stl::size_t i = 0; i < n.value(); ++i) {
        if (out[i].user_token == 111)
            saw_111 = true;
        if (out[i].user_token == 222)
            saw_222 = true;
    }
    EXPECT_TRUE(saw_111);
    EXPECT_TRUE(saw_222);

    ::close(fd1);
    ::close(fd2);
}

TEST(ReactorTest, WakeFromAnotherThreadReturnsImmediately) {
    auto r = Reactor::create();
    ASSERT_TRUE(r.has_value());

    std::atomic<bool> woken{false};
    stl::thread       t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        r.value().wake();
        woken = true;
    });

    Trigger out[8];
    auto    start = std::chrono::steady_clock::now();
    auto    n     = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::seconds(5));
    auto    dt    = std::chrono::steady_clock::now() - start;
    t.join();

    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 0u); // wake events are filtered
    EXPECT_TRUE(woken.load());
    EXPECT_LT(dt, std::chrono::seconds(1));
}

TEST(ReactorTest, WakeBeforeWaitIsCoalesced) {
    auto r = Reactor::create();
    ASSERT_TRUE(r.has_value());

    r.value().wake();
    r.value().wake(); // multiple wakes should not stack

    Trigger out[8];
    auto    n = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::seconds(1));
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 0u);

    // The wake state should be consumed; next wait should block for the timeout.
    auto start = std::chrono::steady_clock::now();
    auto n2    = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(100));
    auto dt    = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(n2.has_value());
    EXPECT_EQ(n2.value(), 0u);
    EXPECT_GE(dt, std::chrono::milliseconds(80)); // mostly slept
}

TEST(ReactorTest, ModifyChangesInterest) {
    auto r  = Reactor::create();
    int  fd = make_eventfd(); // counter=0; not readable, but writable
    ASSERT_GE(fd, 0);

    ASSERT_TRUE(r.value().add(fd, Event::Readable, 1).has_value());

    // No data → no Readable event.
    Trigger out[8];
    auto    n1 = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(50));
    EXPECT_EQ(n1.value(), 0u);

    // Switch interest to Writable. eventfd is always writable when counter < uint64_max.
    ASSERT_TRUE(r.value().modify(fd, Event::Writable, 1).has_value());
    auto n2 = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(50));
    EXPECT_EQ(n2.value(), 1u);
    EXPECT_TRUE(sap::io::has(out[0].events, Event::Writable));

    ::close(fd);
}

TEST(ReactorTest, RemoveStopsEvents) {
    auto r  = Reactor::create();
    int  fd = make_eventfd(1); // counter=1, immediately readable
    ASSERT_GE(fd, 0);

    ASSERT_TRUE(r.value().add(fd, Event::Readable, 1).has_value());

    Trigger out[8];
    auto    n1 = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(50));
    EXPECT_EQ(n1.value(), 1u);
    drain_eventfd(fd);

    EXPECT_TRUE(r.value().remove(fd).has_value());

    // Make it readable again — should not trigger now.
    write_eventfd(fd);
    auto n2 = r.value().wait(stl::span<Trigger>(out, 8), std::chrono::milliseconds(50));
    EXPECT_EQ(n2.value(), 0u);

    ::close(fd);
}

TEST(ReactorTest, AddInvalidFdReturnsError) {
    auto r   = Reactor::create();
    auto add = r.value().add(-1, Event::Readable, 0);
    EXPECT_FALSE(add.has_value());
}

TEST(ReactorTest, RemoveUnregisteredReturnsError) {
    auto r   = Reactor::create();
    int  fd  = make_eventfd();
    auto rm  = r.value().remove(fd);
    EXPECT_FALSE(rm.has_value());
    ::close(fd);
}

TEST(ReactorTest, EmptySpanReturnsZeroWithoutBlocking) {
    auto r = Reactor::create();
    int  fd = make_eventfd(1);
    ASSERT_TRUE(r.value().add(fd, Event::Readable, 1).has_value());

    auto    start = std::chrono::steady_clock::now();
    Trigger empty[1];
    auto    n  = r.value().wait(stl::span<Trigger>(empty, 0), std::chrono::seconds(60));
    auto    dt = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 0u);
    EXPECT_LT(dt, std::chrono::milliseconds(50));

    ::close(fd);
}
