#Compiler
CXX              := g++

TARGET           := fast-chess

# Source, Includes
SRCDIR           := src
TESTDIR          := tests
BUILDDIR         := tmp
INCDIR           := src

#Flags, Libraries and Includes
CXXFLAGS         := -O3 -std=c++17 -Wall -Wextra
INC              := -I$(INCDIR) -Ithird_party

SRC_FILES        := $(shell find $(SRCDIR) -name "*.cpp")
SRC_TEST_FILES   := $(shell find $(TESTDIR) -maxdepth 1 -name "*.cpp")

NATIVE 	         := -march=native

ifeq ($(OS), Windows_NT)
	MKDIR    := mkdir -p
	uname_S  := Windows
	SUFFIX   := .exe
	LDFLAGS  := -static -static-libgcc -static-libstdc++ -Wl,--no-as-needed
else
ifeq ($(COMP), MINGW)
	MKDIR    := mkdir -p
	uname_S  := Windows
	SUFFIX   := .exe
else
	MKDIR   := mkdir -p
	LDFLAGS := -pthread
	uname_S := $(shell uname -s)
endif
endif

ifeq ($(MAKECMDGOALS),$(TESTDIR))
	CXXFLAGS  := -O2 -std=c++17 -Wall -Wextra -pedantic -Wuninitialized -g3 -fno-omit-frame-pointer
	SRC_FILES := $(filter-out src/main.cpp, $(SRC_FILES)) $(SRC_TEST_FILES)
	TARGET    := $(TARGET)-tests
	NATIVE    := 
else
	CXXFLAGS  += -DNDEBUG
endif

OBJECTS   := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRC_FILES))
DEPENDS   := $(patsubst %.cpp,$(BUILDDIR)/%.d,$(SRC_FILES))

ifeq ($(build), debug)
	CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic -Wuninitialized -g3
endif

ifeq ($(build), release)
	LDFLAGS  := -lpthread -static -static-libgcc -static-libstdc++ -Wl,--no-as-needed
	NATIVE   := -march=x86-64
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE =
	LDFLAGS =
endif

# Compile with address sanitizer
ifeq ($(san), asan)
	LDFLAGS += -fsanitize=address
endif

# Compile with memory sanitizer
ifeq ($(san), memory)
	LDFLAGS += -fsanitize=memory -fPIE -pie
endif

# Compile with undefined behavior sanitizer
ifeq ($(san), undefined)
	LDFLAGS += -fsanitize=undefined
endif

# Compile with thread sanitizer
ifeq ($(san), thread)
	LDFLAGS += -fsanitize=thread
endif

# Versioning
GIT_SHA = $(shell git rev-parse --short HEAD 2>/dev/null)
ifneq ($(GIT_SHA), )
	CXXFLAGS += -DGIT_SHA=\"$(GIT_SHA)\"
endif

GIT_DATE = $(shell git show -s --date=format:'%Y%m%d' --format=%cd HEAD 2>/dev/null)
ifneq ($(GIT_DATE), )
	CXXFLAGS += -DGIT_DATE=\"$(GIT_DATE)\"
endif

# Compile with Cutechess output support
ifeq ($(USE_CUTE), true)
	CXXFLAGS += -DUSE_CUTE
endif

.PHONY: clean all tests FORCE

DEPFLAGS         := -MMD -MP

all: $(TARGET)

tests: $(TARGET)
	$(CXX) $(CXXFLAGS) ./tests/mock/engine/dummy_engine.cpp -o ./tests/mock/engine/dummy_engine$(SUFFIX) $(LDFLAGS)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) $(INC) $(DEPFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: %.cpp | build_directories
	$(CXX) $(CXXFLAGS) $(NATIVE) $(INC) $(DEPFLAGS) -c $< -o $@ $(LDFLAGS)

build_directories:
	@$(MKDIR) $(BUILDDIR)
	@find src -type d -exec $(MKDIR) $(BUILDDIR)/{} \;
	@find tests -type d -exec $(MKDIR) $(BUILDDIR)/{} \;

clean:
	rm -rf $(BUILDDIR) $(TARGET) *.exe

-include $(DEPENDS)

# Force rebuild of options.o for accurate versioning
tmp/src/options.o: FORCE
FORCE: