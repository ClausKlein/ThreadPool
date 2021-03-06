// Copyright (C) 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if !defined BOOST_NO_CXX11_DECLTYPE
#    define BOOST_RESULT_OF_USE_DECLTYPE
#else
#    define BOOST_THREAD_VERSION 3
#endif

#ifndef BOOST_THREAD_VERSION
#    warning "BOOST_THREAD_VERSION should be 4!" CK
#endif

#include <boost/thread/future.hpp>

#include <exception>
#include <iostream>

// FIXME: why? CK
namespace boost
{
template <typename T> exception_ptr make_exception_ptr(T v)
{
    return copy_exception(v);
}
} // namespace boost

int f1() { return 5; }

int& f1r()
{
    static int i = 0;
    return i;
}

void p() { }

#if defined BOOST_THREAD_USES_MOVE
boost::future<void> void_compute()
{
    return BOOST_THREAD_MAKE_RV_REF(boost::make_ready_future());
}
#endif

#if BOOST_THREAD_VERSION >= 4
boost::future<int> compute(int x)
{
    if (x == 0)
        return boost::make_ready_future(0);

#    ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
#        warning BOOST_NO_CXX11_RVALUE_REFERENCES
    if (x < 0)
        return boost::make_exceptional_future<int>(std::logic_error("Error"));
#    else
    if (x < 0)
        return boost::make_exceptional(std::logic_error("Error"));
#    endif

    // boost::future<int> f1 = boost::async([]() { return x+1; });
    boost::future<int> f1 = boost::async(boost::launch::async, f1);
    return boost::move(f1);
}
#endif

boost::future<int&> compute_ref(int x)
{
    static int i = 0;
    // if (x == 0) return boost::make_ready_future<int&>(i); //This must not
    // compile as the type is deduced as boost::future<int>
    if (x == 0)
        return boost::make_ready_no_decay_future<int&>(i);

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
    if (x < 0)
        return boost::make_exceptional_future<int&>(std::logic_error("Error"));
#else
    if (x < 0)
        return boost::make_exceptional(std::logic_error("Error"));
#endif

    boost::future<int&> f1 = boost::async(boost::launch::async, f1r);
    return boost::move(f1);
}

#if BOOST_THREAD_VERSION >= 4
boost::shared_future<int> shared_compute(int x)
{
    if (x == 0)
        return boost::make_ready_future(0).share();

#    ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
    if (x < 0)
        return boost::make_exceptional_future<int>(std::logic_error("Error"))
            .share();
#    else
    if (x < 0)
        return boost::make_exceptional(std::logic_error("Error"));
#    endif

    // boost::future<int> f1 = boost::async([]() { return x+1; });
    boost::shared_future<int> f1 =
        boost::async(boost::launch::async, &f1).share();
    return f1;
}
#endif

int main()
{
    const int number_of_tests = 100;
    for (int i = 0; i < number_of_tests; i++)
        try {
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<int> f = boost::async(boost::launch::async, f1);
                std::cout << i << " " << f.get() << std::endl;
            }

#if defined BOOST_THREAD_USES_MOVE
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<void> f = void_compute();
                f.get();
            }
#endif

#if BOOST_THREAD_VERSION >= 4
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<int> f = compute(-1);
                f.wait();
            }
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<int> f = compute(0);
                std::cout << f.get() << std::endl;
            }
#endif

            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<int&> f = compute_ref(0);
                std::cout << f.get() << std::endl;
            }

#if __cplusplus > 201103L
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                int i                 = 0;
                boost::future<int&> f = boost::make_ready_future(std::ref(i));
                std::cout << f.get() << std::endl;
            }
#endif

            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                int i = 0;
                boost::future<int&> f =
                    boost::make_ready_future(boost::ref(i));
                std::cout << f.get() << std::endl;
            }
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                const int i = 0;
                boost::future<int const&> f =
                    boost::make_ready_future(boost::cref(i));
                std::cout << f.get() << std::endl;
            }

#if BOOST_THREAD_VERSION >= 4
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::future<int> f = compute(2);
                std::cout << f.get() << std::endl;
            }
            {
                std::cout << __FILE__ << " " << __LINE__ << std::endl;
                boost::shared_future<int> f = shared_compute(0);
                std::cout << f.get() << std::endl;
            }
#endif

        } catch (std::exception& ex) {
            std::cerr << "ERRORRRRR " << ex.what() << std::endl;
            return 1;
        } catch (...) {
            std::cerr << "ERRORRRRR "
                      << "non std::exception thrown" << std::endl;
            return 2;
        }

    return 0;
}
