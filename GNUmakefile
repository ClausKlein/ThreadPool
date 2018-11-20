#=====================
#   configure part
BOOST_ROOT?=/usr/local
MT?=-mt
CXX:=ccache /usr/bin/g++
ifndef TCOV
CXXFLAGS+=-O2 -DNDEBUG
## CXXFLAGS+=-g
endif
#=====================

#NO! CK
### USE_AGENTPP:=1

CPPFLAGS+=-MMD
CPPFLAGS+=-DBOOST_ALL_NO_LIB
CPPFLAGS+=-DPOSIX_THREADS
CPPFLAGS+=-I$(BOOST_ROOT)/include
LDFLAGS+= -L$(BOOST_ROOT)/lib
LDLIBS:= -lboost_chrono$(MT) -lboost_thread$(MT) -lboost_system$(MT)

CXXFLAGS+=-Wpedantic -Wextra -Wno-unused-parameter -Wno-c++11-long-long -Wno-long-long

PROGRAMS:= \
alarm_cond \
ba_externallly_locked \
chrono_io_ex1 \
default_executor \
enable_shared_from_this \
executor \
perf_shared_mutex \
serial_executor \
shared_mutex \
shared_ptr \
stopwatch_reporter_example \
synchronized_person \
thread_pool \
thread_tss_test \
threads_test \
trylock_test \
volatile \


SRC:=$(PROGRAMS:=.cpp)
OBJ:=$(SRC:.cpp=.o)
DEP:=$(SRC:.cpp=.d)

MAKEFLAGS += -r # --no-buldin-rules
.SUFFIXES:      # no default suffix rules!
#boost unittests links faster without recompile # .INTERMEDIATE: $(OBJ)

ifdef TCOV
CXXFLAGS+=--coverage -DNDEBUG
LDFLAGS+=--coverage
TCOVFLAGS+=--branch-probabilities # --unconditional-branches --all-blocks
LCOVFLAGS+=--rc lcov_branch_coverage=1
#
#TODO: git clone https://github.com/linux-test-project/lcov.git
#
ifdef MSYS
    TCOVFLAGS+=--relative-only --demangled-names --function-summaries
endif
tcov: clean threads_test thread_pool
	./thread_pool
	./threads_test --log_level=error --random
	gcov --long-file-names $(TCOVFLAGS) thread.cpp > /dev/null 2>&1
	lcov --capture --quiet $(LCOVFLAGS) --no-external --directory . --output-file coverage.info
	lcov --list coverage.info $(LCOVFLAGS) | tee gcov-summary.txt
	genhtml coverage.info $(LCOVFLAGS) --demangle-cpp --output-directory html
endif


.PHONY: all cmake ctest tcov test clean distclean cppcheck format
all: $(PROGRAMS)

cmake: build
	cd build && cmake --build .

build: CMakeLists.txt
	mkdir -p build
	cd build && cmake -G Ninja -DBOOST_ROOT=${BOOST_ROOT} ..

ctest: cmake
	cd build && ctest -C debug

# examples using boost libs
lockfree_spsc_queue: CXXFLAGS+=--std=c++03
lockfree_spsc_queue.o: lockfree_spsc_queue.cpp simple_stopwatch.hpp


perf_shared_mutex: CXXFLAGS+=--std=c++03
perf_shared_mutex.o: perf_shared_mutex.cpp simple_stopwatch.hpp

chrono_io_ex1: CXXFLAGS+=--std=c++03
shared_mutex: CXXFLAGS+=--std=c++03
stopwatch_reporter_example: CXXFLAGS+=--std=c++03
thread_tss_test: CXXFLAGS+=--std=c++03


#
# asio demos
#
-async_server: CXXFLAGS+=--std=c++03
-daytime_client: CXXFLAGS+=--std=c++03
-priority_scheduler: CXXFLAGS+=--std=c++03


#
# executer and scheduler demos
#
ba_externallly_locked: CXXFLAGS+=--std=c++03
default_executor: CXXFLAGS+=--std=c++03
executor: CXXFLAGS+=--std=c++03
serial_executor: CXXFLAGS+=--std=c++03
shared_ptr: CXXFLAGS+=--std=c++03
synchronized_person: CXXFLAGS+=--std=c++03
thread_pool: CXXFLAGS+=--std=c++03


# more examples using boost libs
volatile: CXXFLAGS+=--std=c++03
alarm_cond: CXXFLAGS+=--std=c++03
enable_shared_from_this: CXXFLAGS+=--std=c++03


# NOTE: this test_suite using boost unit test framework needs c++14! CK
threads_test.o: CXXFLAGS+=--std=c++14
threads_test.o: threads_test.cpp simple_stopwatch.hpp
threads_test.o: threadpool.hpp


#
# the Agent++V4.1.2 threads.hpp interfaces implemented with boost libs
#
###XXX threadpool.o: CPPFLAGS+=-D_NO_LOGGING
threadpool.o: CXXFLAGS+=--std=c++03
threadpool.o: threadpool.cpp threadpool.hpp

ifdef USE_AGENTPP
threads_test: CPPFLAGS+=-DUSE_AGENTPP
threads_test: LDLIBS+= -lboost_unit_test_framework$(MT)
threads_test: LDLIBS:= -lsnmp++ -lagent++ -lcrypto
threads_test: threads_test.o
else
threads_test: threadpool.o threads_test.o
endif
	$(LINK.cc) $< $@.o -o $@ $(LDLIBS)


#NOTE: plain old posix not longer used!
trylock_test: CXXFLAGS+=--std=c++03
trylock_test.o: trylock_test.cpp simple_stopwatch.hpp
trylock_test: trylock_test.o
	$(LINK.cc) $< -o $@ $(LDLIBS)


# usable for single main sourcefils only! ck
%: %.o
	$(LINK.cc) $< $(LDLIBS) -o $@


%.o: %.cpp
	$(COMPILE.cc) $< -o $@


clean:
	$(RM) $(PROGRAMS) *.o *.exe coverage.info *.gcda *.gcno

distclean: clean
	$(RM) -r build *.d *.bak *.orig *~ *.stackdump *.dSYM

test: $(PROGRAMS)
	./threads_test --log_level=all --run_test='S*'
	./threads_test --log_level=success --random
	./threads_test --run_test=ThreadPool_test -25
	./threads_test --run_test=QueuedThreadPoolLoad_test -25
	#TODO ./threads_test --run_test=QueuedThreadPoolLoad_test -1000
	./chrono_io_ex1
	./default_executor
	#FIXME ./lockfree_spsc_queue
	./perf_shared_mutex
	./shared_mutex
	./shared_ptr
	./stopwatch_reporter_example
	./thread_tss_test
	./trylock_test
	# ./trylock_test +1
	# ./trylock_test -1
	./volatile
	cat alarm_cond.txt | ./alarm_cond --wait

#NOTE: bash for loop:
#	i=0 && while test $$i -lt 1000 && ./threads_test -t QueuedThreadPoolLoad_test ; do \
#	  echo $$i; i=$$(($$i+1)); done

cppcheck:
	cppcheck --enable=all --inconclusive -DBOOST_OVERRIDE=override --std=posix --force -j 2 thread*.cpp

format:
	clang-format -i -style=file thread*.cpp thread*.hpp

ifneq ($(MAKECMDGOALS),distclean)
-include $(DEP)
endif

GNUmakefile::;
%.d::;
%.hpp::;
%.cpp::;


