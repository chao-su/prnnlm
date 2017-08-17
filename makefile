#CC = x86_64-linux-g++-4.6
#CC = x86_64-redhat-linux-g++
CC = x86_64-linux-gnu-g++
WEIGHTTYPE = float
CFLAGS = -D WEIGHTTYPE=$(WEIGHTTYPE) -lm -O2 -Wall -funroll-loops -ffast-math -g
#CFLAGS = -lm -O2 -Wall

all: rnnlmlib.o rnnlm

rnnlmlib.o : rnnlmlib.cpp
	$(CC) $(CFLAGS) $(OPT_DEF) -c rnnlmlib.cpp

rnnlm : rnnlm.cpp
	$(CC) $(CFLAGS) $(OPT_DEF) rnnlm.cpp rnnlmlib.o -o rnnlm

clean:
	rm -rf *.o rnnlm
