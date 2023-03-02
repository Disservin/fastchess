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

ifeq ($(build), release)
	CXXFLAGS := -O3 -std=c++17 -Wall -Wextra -pedantic -DNDEBUG
	LDFLAGS = -lpthread -static -static-libgcc -static-libstdc++ -Wl,--no-as-needed
	NATIVE = -march=x86-64
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

.PHONY: all clean FORCE

SRCDIRS := ./ ./engines ./chess

SOURCES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))
DEPENDS := $(patsubst %.cpp,%.d,$(SOURCES))
TARGET  := fast-chess

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -flto -MMD -MP -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(NATIVE) -flto -MMD -MP -c $< -o $@ $(LDFLAGS)

-include $(DEPENDS)

clean:
	@rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

options.o: FORCE
FORCE: