CXX=g++
CFLAGS= -Wall -g -O -fPIC

.PHONY: all clean

all: scaler_xbr.so
clean:
	rm -f *.o *.so

scaler_xbr.so: xbrz.o scaler_xbr.o 
	$(LINK.c) -shared $^ -o $@

scaler_xbr.o: scaler_xbr.cpp
xbrz.o: xbrz.cpp
