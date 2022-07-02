CXX= g++
CFLAGS= -lxinput

all:
	$(CXX) main.cc -o HoloCureController.exe $(CFLAGS)
