//
// test program for agentpp threadpool
//
// astyle --style=kr thread*.{cpp,hpp}
// clang-format -style=file -i thread*.{cpp,hpp}
//

#ifdef USE_AGENTPP
#include "agent_pp/threads.h" // ThreadPool, QueuedThreadPool
#else
#include "threadpool.hpp" // ThreadPool, QueuedThreadPool
#endif

#define BOOST_TEST_MODULE Threads
#define BOOST_TEST_NO_MAIN
#include <boost/test/included/unit_test.hpp>

#include <boost/atomic.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/queue.hpp>

#include "simple_stopwatch.hpp"

#include <iostream>
#include <string>
#include <vector>

#if !defined BOOST_THREAD_TEST_TIME_MS
#ifdef BOOST_THREAD_PLATFORM_WIN32
#define BOOST_THREAD_TEST_TIME_MS 250
#else
#define BOOST_THREAD_TEST_TIME_MS 75
#endif
#endif

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

typedef boost::atomic_size_t test_counter_t;
typedef boost::lockfree::queue<size_t, boost::lockfree::capacity<20> >
    result_queue_t;

class TestTask : public Agentpp::Runnable {
    typedef boost::mutex lockable_type;
    typedef boost::unique_lock<lockable_type> scoped_lock;

public:
    explicit TestTask(
        const std::string& msg, result_queue_t& rslt, unsigned ms_delay = 11)
        : text(msg)
        , result(rslt)
        , delay(ms_delay)
    {
        scoped_lock l(lock);
        ++counter;
    }

    ~TestTask() BOOST_OVERRIDE
    {
        scoped_lock l(lock);
        --counter;
    }

    void run() BOOST_OVERRIDE
    {
        Agentpp::Thread::sleep((rand() % 3) * delay); // ms

        scoped_lock l(lock);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << " called with: " << text);
        size_t hash = boost::hash_value(text);
        result.push(hash);
        ++run_cnt;
    }

    static size_t run_count()
    {
        scoped_lock l(lock);
        return run_cnt;
    }
    static size_t task_count()
    {
        scoped_lock l(lock);
        return counter;
    }
    static void reset_counter()
    {
        scoped_lock l(lock);
        counter = 0;
        run_cnt = 0;
    }

protected:
    static test_counter_t run_cnt;
    static test_counter_t counter;
    static lockable_type lock;

private:
    const std::string text;
    result_queue_t& result;
    unsigned delay;
};

TestTask::lockable_type TestTask::lock;
test_counter_t TestTask::run_cnt(0);
test_counter_t TestTask::counter(0);

void push_task(Agentpp::ThreadPool* tp)
{
    static result_queue_t result;
    tp->execute(new TestTask(
        "Generate to mutch load.", result, BOOST_THREAD_TEST_TIME_MS));
}

BOOST_AUTO_TEST_CASE(ThreadPool_busy_test)
{
    using namespace Agentpp;
    {
        const size_t stacksize = AGENTPP_DEFAULT_STACKSIZE * 2;
        const size_t threadCount(2UL);
        ThreadPool threadPool(threadCount, stacksize);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == threadCount);
        BOOST_TEST(threadPool.get_stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());
        BOOST_TEST(!threadPool.is_busy());

        // call execute parallel from different task!
        boost::thread threads[threadCount];
        for (size_t i = 0; i < threadCount; ++i) {
            threads[i] = boost::thread(push_task, &threadPool);
            threads[i].detach();
            boost::this_thread::yield();
        }
        boost::this_thread::yield();

#ifdef DEBUG
        BOOST_TEST(threadPool.is_busy());
#endif

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(ThreadPool_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        ThreadPool threadPool(4UL);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 4UL);
        BOOST_TEST(threadPool.get_stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(threadPool.is_idle());

        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("Hallo world!", result));
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("ThreadPool is running!", result));
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("Generate some load.", result));
        BOOST_TEST(!threadPool.is_busy());

        threadPool.execute(new TestTask("Under full load now!", result));
        threadPool.execute(new TestTask("Good by!", result));

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        //############################
        BOOST_TEST(TestTask::task_count() == 0UL);
        //############################
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 5UL, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_busy_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        const size_t stacksize = AGENTPP_DEFAULT_STACKSIZE * 2;
        QueuedThreadPool threadPool(2UL, stacksize);

#if !defined(USE_IMPLIZIT_START)
        threadPool.start(); // NOTE: different to ThreadPool, but this
                            // should not really needed!
#endif

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 2UL);
        BOOST_TEST(threadPool.get_stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());
        BOOST_TEST(!threadPool.is_busy());

        // call execute parallel from different task!
        boost::thread threads[4];
        for (int i = 0; i < 4; ++i) {
            threads[i] = boost::thread(push_task, &threadPool);
            threads[i].detach();
        }
        BOOST_TEST(threadPool.is_busy());

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool queuedThreadPool;

#if !defined(USE_IMPLIZIT_START)
        queuedThreadPool.start(); // NOTE: different to ThreadPool, but this
                                  // should not really needed!
#endif

        BOOST_TEST_MESSAGE(
            "queuedThreadPool.size: " << queuedThreadPool.size());
        BOOST_TEST(queuedThreadPool.size() == 1UL);
        BOOST_TEST(
            queuedThreadPool.get_stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(queuedThreadPool.is_idle());
        BOOST_TEST(!queuedThreadPool.is_busy());

        queuedThreadPool.execute(new TestTask("1 Hi again.", result, 10));
        queuedThreadPool.execute(
            new TestTask("2 Queuing starts.", result, 20));
        queuedThreadPool.execute(
            new TestTask("3 Under full load!", result, 30));
        BOOST_TEST(!queuedThreadPool.is_idle());
        BOOST_TEST(queuedThreadPool.is_busy());

        std::srand(static_cast<unsigned>(
            std::time(0))); // use current time as seed for random generator
        unsigned i = 4;
        do {
            unsigned delay = std::rand() % 100;
            std::string msg(boost::lexical_cast<std::string>(i));
            queuedThreadPool.execute(
                new TestTask(msg + " Queuing ...", result, delay));
        } while (++i < (4 + 6));

        do {
            BOOST_TEST_MESSAGE("queuedThreadPool.queue_length: "
                << queuedThreadPool.queue_length());
            Thread::sleep(500); // NOTE: after more than 1/2 sec! CK
        } while (!queuedThreadPool.is_idle());
        BOOST_TEST(queuedThreadPool.is_idle());
        BOOST_TEST(!queuedThreadPool.is_busy());

        queuedThreadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        //############################
        BOOST_TEST(TestTask::task_count() == 0UL);
        //############################
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 9UL, "All task has to be executed!");
    TestTask::reset_counter();

    BOOST_TEST_MESSAGE("NOTE: checking the order of execution");
    for (size_t i = 1; i < 10; i++) {
        size_t value;
        if (result.pop(value)) {
            if (i >= 4) {
                std::string msg(
                    boost::lexical_cast<std::string>(i) + " Queuing ...");
                //###XXX### BOOST_TEST_MESSAGE("expected msg: " << msg);
                BOOST_TEST(boost::hash_value(value) == boost::hash_value(msg),
                    "expected msg: " << msg);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolLoad_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool defaultThreadPool(1UL);

#if !defined(USE_IMPLIZIT_START)
        defaultThreadPool.start(); // NOTE: different to ThreadPool, but this
                                   // should not really needed!
#endif

        BOOST_TEST(defaultThreadPool.is_idle());
        BOOST_TEST(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE(
            "defaultThreadPool.size: " << defaultThreadPool.size());
        defaultThreadPool.execute(new TestTask("Started ...", result));

        unsigned i = 20;
        do {
            if (i > 5) {
                unsigned delay = std::rand() % 100;
                defaultThreadPool.execute(
                    new TestTask("Running ...", result, delay));
                BOOST_TEST(!defaultThreadPool.is_idle());
                Thread::sleep(i / 2); // ms
            }
            BOOST_TEST_MESSAGE("defaultThreadPool.queue_length: "
                << defaultThreadPool.queue_length());
        } while (--i > 0);

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(100); // ms
        } while (!defaultThreadPool.is_idle());
        BOOST_TEST(defaultThreadPool.is_idle());
        BOOST_TEST(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST(TestTask::task_count() == 0UL);

        // NOTE: implicit done: defaultThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 16UL, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolInterface_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool emptyThreadPool(
            0UL, 0x20000); // NOTE: without any worker thread! CK
        BOOST_TEST(emptyThreadPool.size() == 0UL);

#if !defined(USE_AGENTPP) && defined(USE_IMPLIZIT_START)
        BOOST_TEST(emptyThreadPool.is_idle());
        emptyThreadPool.stop();
#endif

        emptyThreadPool.set_stack_size(
            0x20000); // NOTE: this change the queue thread only! CK
        BOOST_TEST(emptyThreadPool.get_stack_size() == 0x20000);

        BOOST_TEST_MESSAGE("emptyThreadPool.size: " << emptyThreadPool.size());
        emptyThreadPool.execute(new TestTask("Starting ...", result));
        BOOST_TEST(emptyThreadPool.is_busy());

#if !defined(USE_IMPLIZIT_START)
        emptyThreadPool.start();
#endif

        BOOST_TEST(!emptyThreadPool.is_idle());

        size_t i = 10;
        do {
            if (i > 5) {
                emptyThreadPool.execute(new TestTask("Running ...", result));
            }
            BOOST_TEST_MESSAGE("emptyThreadPool.queue_length: "
                << emptyThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        BOOST_TEST(!emptyThreadPool.is_idle());
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST(TestTask::task_count() == 6UL);

        // NOTE: implicit done: emptyThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 0UL, "NO task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolIndependency_test)
{
    using namespace Agentpp;

    result_queue_t result;
    QueuedThreadPool firstThreadPool(1);
    BOOST_TEST(firstThreadPool.size() == 1UL);

    BOOST_TEST_MESSAGE("firstThreadPool.size: " << firstThreadPool.size());
    firstThreadPool.execute(new TestTask("Starting ...", result));

#if !defined(USE_IMPLIZIT_START)
    firstThreadPool.start();
    BOOST_TEST(!firstThreadPool.is_idle());
#endif

    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    BOOST_TEST(firstThreadPool.is_idle());
    size_t n = 1;

    {
        QueuedThreadPool secondThreadPool(4);
        BOOST_TEST_MESSAGE(
            "secondThreadPool.size: " << secondThreadPool.size());

#if !defined(USE_IMPLIZIT_START)
        secondThreadPool.start();
#endif

        BOOST_TEST(secondThreadPool.is_idle());

        secondThreadPool.execute(new TestTask("Starting ...", result));
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        BOOST_TEST(secondThreadPool.is_idle());
        n++;

        secondThreadPool.terminate();
        secondThreadPool.execute(new TestTask("After terminate ...", result));
        // NO! n++; //XXX
        Thread::sleep(10); // ms

        size_t i = 10;
        do {
            if (i > 5) {
                secondThreadPool.execute(new TestTask("Queuing ...", result));
                // NO! n++; //XXX
            }
            BOOST_TEST_MESSAGE("secondThreadPool.queue_length: "
                << secondThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        if (!secondThreadPool.is_idle()) {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
        }

        BOOST_TEST(TestTask::run_count() == n);
    }

    firstThreadPool.execute(new TestTask("Stopping ...", result));
    BOOST_TEST_MESSAGE(
        "firstThreadPool.queue_length: " << firstThreadPool.queue_length());
    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    BOOST_TEST(firstThreadPool.is_idle());
    firstThreadPool.terminate();

    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(
        TestTask::run_count() == (n + 1UL), "All task has to be executed!");

    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(Synchronized_test)
{
    using namespace Agentpp;
    Synchronized sync;
    {
        BOOST_TEST(sync.lock());
        BOOST_TEST(sync.unlock());
        BOOST_TEST(!sync.unlock(), "second unlock() returns no error");
    }
    BOOST_TEST(
        !sync.unlock(), "unlock() without previous lock() returns no error");
}

BOOST_AUTO_TEST_CASE(SyncTrylock_test)
{
    using namespace Agentpp;
    Synchronized sync;
    {
        Lock l(sync);
        BOOST_TEST(sync.trylock() == Synchronized::OWNED);
    }
    BOOST_TEST(!sync.unlock(), "second unlock() returns no error");
}

BOOST_AUTO_TEST_CASE(SyncDeadlock_test)
{
    using namespace Agentpp;
    Synchronized sync;
    try {
        Lock l(sync);
        BOOST_TEST(!sync.lock());
    } catch (std::exception& e) {
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);
        BOOST_TEST_MESSAGE(e.what());
        // XXX BOOST_TEST(false);
    }
    BOOST_TEST(sync.lock());
    BOOST_TEST(sync.unlock());
}

BOOST_AUTO_TEST_CASE(SyncWait_test)
{
    using namespace Agentpp;
    Synchronized sync;
    {
        Lock l(sync);
        Stopwatch sw;
        BOOST_TEST(!sync.wait(BOOST_THREAD_TEST_TIME_MS),
            "no timeout occurred on wait!");

        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
}

class BadTask : public Agentpp::Runnable {
public:
    BadTask(){};
    void run()
    {
        std::cout << "Hello world!" << std::endl;
        throw std::runtime_error("Fatal Error, can't continue!");
    };
};

BOOST_AUTO_TEST_CASE(ThreadTaskThrow_test)
{
    using namespace Agentpp;
    Stopwatch sw;
    {
        Thread thread(new BadTask());
        thread.start();
        BOOST_TEST(thread.is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}

BOOST_AUTO_TEST_CASE(ThreadLivetime_test)
{
    using namespace Agentpp;
    Stopwatch sw;
    {
        Thread thread;
        boost::shared_ptr<Thread> ptrThread(thread.clone());
        thread.start();
        BOOST_TEST(thread.is_alive());
        BOOST_TEST(!ptrThread->is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}

BOOST_AUTO_TEST_CASE(ThreadSleep_test)
{
    using namespace Agentpp;
    {
        Stopwatch sw;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
    {
        Stopwatch sw;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS, 999999); // ms + ns
        ns d = sw.elapsed() - (ms(BOOST_THREAD_TEST_TIME_MS) + ns(999999));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
}

struct wait_data {
    typedef boost::mutex lockable_type;
    typedef boost::unique_lock<lockable_type> scoped_lock;

    bool flag;
    lockable_type mtx;
    boost::condition_variable cond;

    wait_data()
        : flag(false)
    {}

    // NOTE: return false if condition waiting for is not true! CK
    bool predicate() { return flag; }

    void wait()
    {
        scoped_lock l(mtx);
        while (!predicate()) {
            cond.wait(l);
        }
    }

    template <typename Duration> bool timed_wait(Duration d)
    {
        scoped_lock l(mtx);
        while (!predicate()) {
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                return false;
            }
        }
        return true; // OK
    }

    void signal()
    {
        scoped_lock l(mtx);
        flag = true;
        cond.notify_all();
    }
};

typedef Agentpp::Synchronized mutex_type;

void lock_mutexes_slowly(
    mutex_type* m1, mutex_type* m2, wait_data* locked, wait_data* quit)
{
    using namespace boost;

    lock_guard<mutex_type> l1(*m1);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l2(*m2);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    locked->signal();
    quit->wait();
}

void lock_pair(mutex_type* m1, mutex_type* m2)
{
    using namespace boost;

    boost::lock(*m1, *m2);
    unique_lock<mutex_type> l1(*m1, adopt_lock), l2(*m2, adopt_lock);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    BOOST_TEST(l1.owns_lock());
    BOOST_TEST(l2.owns_lock());
}

BOOST_AUTO_TEST_CASE(test_lock_two_other_thread_locks_in_order)
{
    using namespace boost;

    mutex_type m1, m2;
    wait_data locked;
    wait_data release;

    thread t(lock_mutexes_slowly, &m1, &m2, &locked, &release);

    thread t2(lock_pair, &m1, &m2);
    BOOST_TEST(locked.timed_wait(ms(2 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(t2.try_join_for(ms(4 * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

BOOST_AUTO_TEST_CASE(test_lock_two_other_thread_locks_in_opposite_order)
{
    using namespace boost;

    mutex_type m1, m2;
    wait_data locked;
    wait_data release;

    thread t(lock_mutexes_slowly, &m1, &m2, &locked, &release);

    thread t2(lock_pair, &m2, &m1); // NOTE: m2 first!
    BOOST_TEST(locked.timed_wait(ms(2 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(t2.try_join_for(ms(4 * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

void lock_five_mutexes_slowly(mutex_type* m1, mutex_type* m2, mutex_type* m3,
    mutex_type* m4, mutex_type* m5, wait_data* locked, wait_data* quit)
{
    using namespace boost;

    lock_guard<mutex_type> l1(*m1);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l2(*m2);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l3(*m3);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l4(*m4);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l5(*m5);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    locked->signal();
    quit->wait();
}

void lock_n(mutex_type* mutexes, unsigned count)
{
    using namespace boost;

    boost::lock(mutexes, mutexes + count);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    if (count == 1) {
        Stopwatch sw;
        BOOST_TEST(mutexes[0].wait(BOOST_THREAD_TEST_TIME_MS));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(mutexes[0].unlock());
        return;
    }

    for (unsigned i = 0; i < count; ++i) {
        ms d(BOOST_THREAD_TEST_TIME_MS);
        this_thread::sleep_for(d);
        BOOST_TEST(mutexes[i].unlock());
    }
}

BOOST_AUTO_TEST_CASE(test_lock_ten_other_thread_locks_in_different_order)
{
    using namespace boost;
    unsigned const num_mutexes = 10;

    mutex_type mutexes[num_mutexes];
    wait_data locked;
    wait_data release;

    thread t(lock_five_mutexes_slowly, &mutexes[6], &mutexes[3], &mutexes[8],
        &mutexes[0], &mutexes[2], &locked, &release);

    thread t2(lock_n, mutexes, num_mutexes);
    BOOST_TEST(locked.timed_wait(ms(5 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(
        t2.try_join_for(ms(2 * num_mutexes * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

BOOST_AUTO_TEST_CASE(SyncTry_lock_for_test)
{
    using namespace Agentpp;
    unsigned const num_mutexes = 2;

    Synchronized timed_locks[num_mutexes];
    {
        const unsigned timeout = BOOST_THREAD_TEST_TIME_MS / num_mutexes;
        boost::thread t1(lock_n, timed_locks, num_mutexes);
        boost::this_thread::sleep_for(ms(timeout));
        Stopwatch sw;
        BOOST_TEST(
            !timed_locks[0].lock(timeout), "no timeout occurred on lock!");
        ns d = sw.elapsed() - ms(timeout);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
        t1.join();
    }
    {
        Stopwatch sw;
        BOOST_TEST(timed_locks[0].lock(BOOST_THREAD_TEST_TIME_MS),
            "timeout occurred on lock!");
        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST(d < ns(max_diff));
        BOOST_TEST(timed_locks[0].unlock());
    }
}

BOOST_AUTO_TEST_CASE(SyncDelete_while_used_test)
{
    using namespace Agentpp;
    unsigned const num_mutexes = 1;

    Synchronized* lockable = new Synchronized;
    boost::thread t1(lock_n, lockable, num_mutexes);
    {
        Stopwatch sw;
        boost::this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS / 2));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        delete lockable;
    }
    t1.join();
    {
        Stopwatch sw;
        Synchronized* lockable = new Synchronized;
        lockable->lock();
        delete lockable;
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
    }
}

int main(int argc, char* argv[])
{
    // prototype for user's unit test init function
    extern ::boost::unit_test::test_suite* init_unit_test_suite(
        int argc, char* argv[]);
    int loops                                       = 1;
    int error                                       = 0;
    boost::unit_test::init_unit_test_func init_func = &init_unit_test_suite;
    std::vector<std::string> args;

    if (argc > 1 && argv[argc - 1][0] == '-') {
        std::stringstream ss(argv[argc - 1] + 1);
        ss >> loops;
        if (!ss.fail()) {
            std::cout << "loops requested: " << loops << std::endl;
            --argc; // private args not for boost::unit_test! CK
        }
        for (int i = 0; i < argc; ++i) {
            args.push_back(std::string(argv[i]));
            std::cout << i << ": " << argv[i] << std::endl;
        }
    }

    do {
        StopwatchReporter sw;

        error = ::boost::unit_test::unit_test_main(init_func, argc, argv);

        if (--loops <= 0) {
            break;
        }
        for (int i = 0; i < argc; ++i) {
            strcpy(argv[i], args[i].c_str());
            std::cout << i << ": " << argv[i] << std::endl;
        }
        std::cout << "loops left: " << loops << std::endl;
    } while (!error);

    return error;
}
