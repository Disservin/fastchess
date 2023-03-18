CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -DNDEBUG
INCLUDES = -Isrc
DEPFLAGS = -MMD -MP
BUILDDIR = build

# Detect Windows
ifeq ($(OS), Windows_NT)
	MKDIR    := mkdir
	uname_S  := Windows
	SUFFIX   := .exe
else
ifeq ($(COMP), MINGW)
	MKDIR    := mkdir
	uname_S  := Windows
	SUFFIX   := .exe
else
	MKDIR   := mkdir -p
	LDFLAGS := -pthread
	uname_S := $(shell uname -s)
	SUFFIX  :=
endif
endif

ifeq ($(build), debug)
	CXXFLAGS := -g3 -O3 -std=c++17 -Wall -Wextra -pedantic
endif

ifeq ($(build), release)
	CXXFLAGS := -O3 -std=c++17 -Wall -Wextra -pedantic -DNDEBUG
	LDFLAGS = -lpthread -static -static-libgcc -static-libstdc++ -Wl,--no-as-needed
	NATIVE = -march=x86-64
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE =	
	LDFLAGS =
endif

ifeq ($(MAKECMDGOALS),tests)
	CXXFLAGS  := -O2 -std=c++17 $(INCLUDES) -g3 -fno-omit-frame-pointer -Wall -Wextra
	TEST_SR   := $(wildcard *.cpp) $(wildcard */*.cpp) $(wildcard */*/*.cpp)
	SRC_FILES := $(filter-out src/main.cpp, $(TEST_SR))
	OBJECTS   := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRC_FILES))
	DEPENDS   := $(patsubst %.cpp,$(BUILDDIR)/%.d,$(SRC_FILES))
	TARGET    := fast-chess-tests
else
	CXXFLAGS  += $(INCLUDES)
	NATIVE 	  := -march=native
	SRC_FILES := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)
	OBJECTS   := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRC_FILES))
	DEPENDS   := $(patsubst %.cpp,$(BUILDDIR)/%.d,$(SRC_FILES))
	TARGET    := fast-chess
endif

ifeq ($(san), asan)
	LDFLAGS += -fsanitize=address
endif

ifeq ($(san), memory)
	LDFLAGS += -fsanitize=memory -fPIE -pie
endif

ifeq ($(san), undefined)
	LDFLAGS += -fsanitize=undefined
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

.PHONY: clean all tests FORCE

all: $(TARGET)

tests: $(TARGET)
	$(CXX) $(CXXFLAGS) ./tests/data/engine/dummy_engine.cpp -o ./tests/data/engine/dummy_engine$(SUFFIX)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@ $(LDFLAGS)

$(BUILDDIR):
	$(MKDIR) "$(BUILDDIR)" "$(BUILDDIR)/src" "$(BUILDDIR)/src/engines" "$(BUILDDIR)/src/chess" "$(BUILDDIR)/tests" "$(BUILDDIR)/src/matchmaking"

clean:
	rm -rf $(BUILDDIR)

-include $(DEPENDS)

build/src/options.o: FORCE
FORCE: