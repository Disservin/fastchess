CXX 	 := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra -pedantic -DNDEBUG
NATIVE 	 := -march=native

# Detect Windows
ifeq ($(OS), Windows_NT)
	uname_S  := Windows
else
ifeq ($(COMP), MINGW)
	uname_S  := Windows
else
	LDFLAGS := -pthread
	SUFFIX  :=
	uname_S := $(shell uname -s)
endif
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE = -mcpu=apple-a14	
	LDFLAGS = 
endif

ifeq ($(build), debug)
	CXXFLAGS := -g3 -O3 -std=c++17 -Wall -Wextra -pedantic
endif

SOURCES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))
DEPENDS := $(patsubst %.cpp,%.d,$(SOURCES))
TARGET  := fast-chess

.PHONY: all clean FORCE

all: $(TARGET)
clean:
	@rm -rf *.o $(DEPENDS) *.d

# Linking the executable from the object files
$(TARGET): $(OBJECTS)
	$(CXX) -o $(TARGET) $^ $(CXXFLAGS) $(NATIVE) $(FLAGS) -flto $(LDFLAGS)

-include $(DEPENDS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(NATIVE) -flto -MMD -MP -c -o $@ $< $(LDFLAGS)
