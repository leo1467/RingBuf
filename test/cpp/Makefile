TARGET=test

SRCFILES=$(wildcard *.c *.cc *.cpp)
OBJS=$(patsubst %.c, %.o, $(SRCFILES))

CC=g++

# CCFLAG=-Wall -g -Ofast -std=c++17 -fconcepts
CCFLAG=-Wall -g -Ofast -std=c++17

USR_LOCAL_LIB=${HOME}/.local

INCLUDEDIR=-I. \
		   -I${HOME}/workspace/NuLib/include \
		   -I../../include

LIBDIR=-L${HOME}/workspace/NuLib/bin \
	   -L../../bin/static

LIBS=-lpthread -lNuUtil -D_GNU_SOURCE -lRingBuf

COMPILE=$(CC) $(CCFLAG) $(INCLUDEDIR)

all: clean $(TARGET)

$(TARGET):$(OBJS)
	@echo === LINK $^ ===
	$(COMPILE) $^ $(LIBDIR) $(LIBS) -o $@

%.o:%.cpp
	@echo === COMPILE $@ ===
	$(COMPILE) $(INCLUDEDIR) -c $< -o $@
	@echo .

s:$(SRCFILES)
	$(COMPILE) $(INCLUDEDIR) -S -Og -masm=intel -fverbose-asm $< -o main.s

e:$(SRCFILES)
	$(COMPILE) $(INCLUDEDIR) -E $< -o main.i

clean:
	-rm -f $(TARGET) *.o
