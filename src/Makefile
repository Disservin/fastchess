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

# Versioning
GIT_SHA = $(shell git rev-parse --short HEAD 2>/dev/null)
ifneq ($(GIT_SHA), )
	CXXFLAGS += -DGIT_SHA=\"$(GIT_SHA)\"
endif

# Versioning
GIT_DATE = $(shell git show -s --date=format:'%Y%m%d' --format=%cd HEAD 2>/dev/null)
ifneq ($(GIT_DATE), )
	CXXFLAGS += -DGIT_DATE=\"$(GIT_DATE)\"
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

options.o: FORCE
FORCE:
