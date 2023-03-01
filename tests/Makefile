CXX      = g++
CXXFLAGS = -O3 -std=c++17 -g3
NATIVE   = -march=native
SUFFIX   = .exe

# Detect Windows
ifeq ($(OS), Windows_NT)
	uname_S  := Windows
else
ifeq ($(COMP), MINGW)
	uname_S  := Windows
else
	LDFLAGS := -pthread
	uname_S := $(shell uname -s)
	SUFFIX  :=
endif
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE = -mcpu=apple-a14	
	LDFLAGS = 
endif

ALL_SOURCES = $(wildcard ../src/*.cpp ../src/chess/*.cpp ../src/engines/*.cpp *.cpp)
SOURCES = $(filter-out ../src/main.cpp, $(ALL_SOURCES))

TARGET = fast-chess-tests$(SUFFIX)

all:
	$(CXX) $(CXXFLAGS) $(NATIVE) ./data/engine/dummy_engine.cpp -o ./data/engine/dummy_engine $(LDFLAGS)
	$(CXX) $(CXXFLAGS) $(NATIVE) $(SOURCES) -o $(TARGET) $(LDFLAGS)
