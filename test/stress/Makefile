TARGET=producer consumer

SRCFILES=$(wildcard *.c)
OBJS=$(patsubst %.c, %.o, $(SRCFILES))

CC=gcc

CCFLAG=-Wall -g -Ofast -DDEBUG

USR_LOCAL_LIB=${HOME}/.local

INCLUDEDIR=-I. \
		   -I$(HOME)/workspace/NuLib/include \
		   -I../../include

LIBDIR=-L$(HOME)/workspace/NuLib/bin \
	   -L../../bin/static

LIBS=-lpthread -lNuUtil -D_GNU_SOURCE -lRingBuf

COMPILE=$(CC) $(CCFLAG)

all: clean $(TARGET)

producer:producer.o
	@echo === LINK $^ ===
	$(COMPILE) $^ $(LIBDIR) $(LIBS) -o $@

consumer:consumer.o
	@echo === LINK $^ ===
	$(COMPILE) $^ $(LIBDIR) $(LIBS) -o $@

%.o:%.c
	@echo === COMPILE $@ ===
	$(COMPILE) $(INCLUDEDIR) -c $< -o $@
	@echo .

clean:
	-rm -f $(TARGET) *.o
