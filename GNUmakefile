#
# Standard stuff
#
.SUFFIXES:

# Disable the built-in implicit rules.
MAKEFLAGS+= --no-builtin-rules

.PHONY: setup show all test lcov install check format clean distclean

PROJECT_NAME:=$(shell basename $(CURDIR))

##################################################
# begin of config part
# see https://www.kdab.com/clang-tidy-part-1-modernize-source-code-using-c11c14/
# and https://github.com/llvm-mirror/clang-tools-extra/blob/master/clang-tidy/tool/run-clang-tidy.py
#
### checkAllHeader:='include/spdlog/[acdlstv].*'
## checkAllHeader?='include/spdlog/[^f].*'
checkAllHeader?='$(CURDIR)/.*'

# NOTE: there are many errors with boost::test, doctest, catch test framework! CK
CHECKS:='-*,cert-*,-cert-err58-cpp,hicpp-*,-hicpp-use-*,-hicpp-special-*,-hicpp-no-array-decay,-hicpp-vararg,misc-*,-misc-unused-*,-misc-no-recursion,modernize-use-override,performance-*,portability-*,readability-*,-readability-implicit-*,-readability-magic-* '
CHECKS?='-cppcoreguidelines-avoid-c-arrays,-modernize-avoid-c-arrays'
CHECKS?='-*,cppcoreguidelines-*,cppcoreguidelines-pro-*,-cppcoreguidelines-avoid-*'
CHECKS?='-*,portability-*,readability-*'
CHECKS?='-*,misc-*,boost-*,cert-*,misc-unused-parameters'

#FIXME! ThreadSanitizer?=0
ifeq ($(BUILD_TYPE),Coverage)
    ThreadSanitizer:=0
else
   ThreadSanitizer?=1
endif

# prevent hard config of find_package(asio 1.14.1 CONFIG CMAKE_FIND_ROOT_PATH_BOTH)
ifeq (${ThreadSanitizer},1)
    #XXX CC:=/usr/local/opt/llvm/bin/clang
    #XXX CXX:=/usr/local/opt/llvm/bin/clang++
    CC:=clang
    CXX:=clang++

    CMAKE_INSTALL_PREFIX?=/usr/local
    export CMAKE_INSTALL_PREFIX
    CMAKE_STAGING_PREFIX?=/tmp/staging/$(PROJECT_NAME)$(CMAKE_INSTALL_PREFIX)
    CMAKE_PREFIX_PATH?="$(CMAKE_STAGING_PREFIX);$(CMAKE_INSTALL_PREFIX);/usr/local/opt/boost;/usr/local/opt/openssl;/usr"
endif


#NO! BUILD_TYPE?=Coverage
# NOTE: use on shell$> BUILD_TYPE=Coverage make lcov
BUILD_TYPE?=Debug
BUILD_TYPE?=Release

# GENERATOR:=Xcode
GENERATOR?=Ninja

# end of config part
##################################################


BUILD_DIR:=../build-$(PROJECT_NAME)-${CROSS_COMPILE}$(BUILD_TYPE)
ifeq ($(BUILD_TYPE),Coverage)
    USE_LOCV=ON
    ifeq (NO${CROSS_COMPILE},NO)
        CC:=/usr/bin/gcc
        CXX:=/usr/bin/g++
    endif
else
    USE_LOCV=OFF
endif

all: setup .configure-$(BUILD_TYPE)
	cmake --build $(BUILD_DIR)

test: all
	cd $(BUILD_DIR) && ctest -C $(BUILD_TYPE) --timeout 60 --rerun-failed --output-on-failure .
	cd $(BUILD_DIR) && ctest -C $(BUILD_TYPE) --timeout 60 .


check:: setup .configure-$(BUILD_TYPE) compile_commands.json
	run-clang-tidy.py -header-filter=$(checkAllHeader) -checks=$(CHECKS) | tee run-clang-tidy.log 2>&1
	egrep '\b(warning|error):' run-clang-tidy.log | perl -pe 's/(^.*) (warning|error):/\2/' | sort -u

setup: $(BUILD_DIR) .clang-tidy compile_commands.json
	find . -name CMakeLists.txt -o -name '*.cmake' -o -name '*.cmake.in' -o -name '*meson*' > .buildfiles.lst

.configure-$(BUILD_TYPE): CMakeLists.txt
	cd $(BUILD_DIR) && cmake -G $(GENERATOR) -Wdeprecated -Wdev \
      -DUSE_LCOV=$(USE_LOCV) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
      -DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH) \
      -DCMAKE_STAGING_PREFIX=$(CMAKE_STAGING_PREFIX) -DUSE_ThreadSanitizer=${ThreadSanitizer}\
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} $(CURDIR)
	touch $@

compile_commands.json: .configure-$(BUILD_TYPE)
	ln -sf $(BUILD_DIR)/compile_commands.json .

$(BUILD_DIR): GNUmakefile
	mkdir -p $@

format: .clang-format
	find . -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -style=file -i

show: setup
	cmake -S $(CURDIR) -B $(BUILD_DIR) -L

check:: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target $@ | tee run-clang-tidy.log 2>&1
	egrep '\b(warning|error):' run-clang-tidy.log | perl -pe 's/(^.*) (warning|error):/\2/' | sort -u

lcov: all .configure-Coverage
	cmake --build $(BUILD_DIR) --target $@

install: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target $@

clean: $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target $@

distclean:
	rm -rf $(BUILD_DIR) .configure-$(BUILD_TYPE) .buildfiles.lst compile_commands.json *~ .*~ tags
	find . -name '*~' -delete
	#TODO rm -rf generated/*


# These rules keep make from trying to use the match-anything rule below
# to rebuild the makefiles--ouch!

## CMakeLists.txt :: ;
GNUmakefile :: ;
.clang-tidy :: ;
.clang-format :: ;

# Anything we don't know how to build will use this rule.  The command is
# a do-nothing command.
% :: ;

