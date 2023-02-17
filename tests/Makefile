CXX = g++
CXXFLAGS = -O3 -std=c++17
NATIVE = -march=native

# Detect Windows
ifeq ($(OS), Windows_NT)
	uname_S  := Windows
else
ifeq ($(COMP), MINGW)
	uname_S  := Windows
else
	LDFLAGS := -static -lpthread -lstdc++
	SUFFIX  :=
	uname_S := $(shell uname -s)
endif
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE = -mcpu=apple-a14	
	LDFLAGS = 
endif

SRCS = ../src/board.cpp ../src/engine.cpp ../src/match.cpp ../src/options.cpp ../src/helper.cpp

all:
	$(CXX) $(CXXFLAGS) $(NATIVE) *.cpp $(SRCS) -o fast-chess_tests $(LDFLAGS)