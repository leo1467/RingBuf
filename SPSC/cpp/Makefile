#
# @(#)Makefile
#
# Modification History:
#
#
# Makefile for Linux platform
TMPDIR=../tmp/
BINDIR_DYNAMIC=../bin/dynamic/
BINDIR_STATIC=../bin/static/
BINDIR=$(BINDIR_STATIC)
HEADERDIR=../include/

LINK_TYPE=static

ScanTarget=ShmRingBufcpp

CC=g++

AR=ar
AFLAG=rcs

SRCFILES=$(wildcard *.c *.cc *.cpp)
HEADERS=$(wildcard *.h *.hpp)
INCLUDEDIRFILES=$(addprefix $(HEADERDIR), $(HEADERS))

OBJS=$(patsubst %, $(TMPDIR)%, $(patsubst %.c, %.o, $(patsubst %.cc, %.o, $(SRCFILES))))

TARGET_STATIC=libShmRingBufcpp.a
TARGET_DYNAMIC=libShmRingBufcpp.so.1.0.0
LINK1=libShmRingBufcpp.so.1
LINK2=libShmRingBufcpp.so

TARGET?=$(TARGET_STATIC)

BUILD?=release
BUILD_TYPE?=static

DEBUG_CCFLAG=-O0 -DDEBUG

RELEASE_CCFLAG=-Ofast

CCFLAG=-Wall -Wextra -Wno-parentheses \
	   -std=c++17 -g -finline-functions -finline-limit=3600 -m64 -fno-builtin-malloc -ldl -rdynamic -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE -D_GNU_SOURCE

INCLUDEDIR=\
		-I. \
		-I../c

LIBDIR=\

LIBS=

LDFLAGS_DYNAMIC=-fpic -shared -Wl,-soname,$(LINK1)
LDFLAGS_STATIC=-static
LDFLAG=

ifeq ($(BUILD), debug)
	CCFLAG += $(DEBUG_CCFLAG)
else ifeq ($(BUILD), release)
	CCFLAG += $(RELEASE_CCFLAG)
endif

ifeq ($(BUILD_TYPE), dynamic)
	TARGET=$(TARGET_DYNAMIC)
	LDFLAG+=$(LDFLAGS_DYNAMIC)
	BINDIR=$(BINDIR_DYNAMIC)
	LINK=$(CC) $(CCFLAG) $(LDFLAGS_DYNAMIC) -o $@ $^
else ifeq ($(BUILD_TYPE), static)
	TARGET=$(TARGET_STATIC)
	LDFLAG+=$(LDFLAGS_STATIC)
	BINDIR=$(BINDIR_STATIC)
	LINK=$(AR) $(AFLAG) $@ $^
endif

COMPILE+=$(CC) $(CCFLAG) $(LDFLAG)

all: init
	@echo "---- end of build ----"

init:
	@mkdir -p $(TMPDIR) $(BINDIR) $(HEADERDIR)

$(TMPDIR)$(TARGET): $(OBJS)
	@echo "---- build $@ ----"
	$(LINK)
	@echo .

$(TMPDIR)%.o: %.c
	@echo "---- Compile $< ----"
	$(COMPILE) $(INCLUDEDIR) -c $< -o $@
	@echo .

$(TMPDIR)%.o: %.cc
	@echo "---- Compile $< ----"
	$(COMPILE) $(INCLUDEDIR) -c $< -o $@
	@echo .

$(TMPDIR)%.o: %.cpp
	@echo "---- Compile $< ----"
	$(COMPILE) $(INCLUDEDIR) -c $< -o $@
	@echo .

clean:
	@echo "Clean up..."
	rm -f $(HEADERDIR)*
	rm -f $(TMPDIR)*
	rm -rf ../bin/*

install: all LINK1 LINK2
	@if [ -n "$(HEADERDIR)" ]; then \
		if [ -n "$(HEADERS)" ]; then \
			echo "--- install headers ... $(HEADERS)"; \
			echo "cp -p $(HEADERS) $(HEADERDIR)"; \
			cp -p $(HEADERS) $(HEADERDIR); \
		fi \
	fi
	@if [ -f $(TMPDIR)$(TARGET) ]; then \
		echo "--- install binaries ... $(TARGET)"; \
		echo "cp -p $(TMPDIR)$(TARGET) $(BINDIR)"; \
		cp -p $(TMPDIR)$(TARGET) $(BINDIR); \
	fi


LINK1:
	@if [ "${BUILD_TYPE}x" = "dynamicx" ]; then \
		cd $(BINDIR); \
		ln -sf $(TARGET) $(LINK1); \
	fi

LINK2:
	@if [ "${BUILD_TYPE}x" = "dynamicx" ]; then\
		cd $(BINDIR); \
		ln -sf $(LINK1) $(LINK2); \
	fi

scan: 
	@echo "---- fortify scan $(ScanTarget) ----"; \
	d=$$(date +%s); \
	sourceanalyzer -b $(ScanTarget) -clean; \
	make clean; sourceanalyzer -Xmx4G -Xss1G -b $(ScanTarget) make -j 10 install; \
	sourceanalyzer -Xmx4G -Xss1G -f ../$(ScanTarget).fpr -b $(ScanTarget) -scan -mt; \
    echo "Build and Scan took $$(($$(date +%s)-d)) seconds"

debug:
	$(MAKE) BUILD=debug

release:
	$(MAKE) BUILD=release

.PHONY: all init clean install
