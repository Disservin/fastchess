CXX 	 := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra -pedantic -DNDEBUG
NATIVE 	 := -march=native

# Detect Windows
ifeq ($(OS), Windows_NT)
	UNAME_S  := Windows
	TMPDIR   := .tmp
	TMPLOC   = .tmp\engines .\.tmp\chess
	MKDIR_P  = cmd //C md
	RM_F 	 = cmd //C del //Q //F
    RM_RF 	 = cmd //C rmdir //Q //S
else
	UNAME_S  := $(shell uname -s)
	TMPDIR   := .tmp
	TMPLOC   = $(TMPDIR)/engines $(TMPDIR)/chess
	MKDIR_P  = mkdir -p
	RM_F	 = rm -f
	RM_RF	 = rm -rf
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

SRCDIRS := ./ engines chess

SOURCES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJECTS := $(patsubst %.cpp,$(TMPDIR)/%.o,$(SOURCES))
DEPENDS := $(patsubst %.cpp,$(TMPDIR)/%.d,$(SOURCES))
TARGET  := fast-chess

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -flto -MMD -MP -o $@ $^ $(LDFLAGS)

$(TMPDIR)/%.o: %.cpp | $(TMPDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -flto -MMD -MP -c $< -o $@ $(LDFLAGS)

$(TMPDIR):
	@$(MKDIR_P) $(TMPLOC)

-include $(DEPENDS)

clean:
	@$(RM_RF) $(TMPDIR)

options.o: FORCE
FORCE: