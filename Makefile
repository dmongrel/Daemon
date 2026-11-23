# Daemon screensaver Makefile
# Win32 + Direct2D/DirectWrite + WIC + GDI+. MSYS2 UCRT64 toolchain.
# Build from an MSYS2 UCRT64 shell, or from Git Bash with the UCRT64 bin on PATH:
#   PATH=/c/msys64/ucrt64/bin:$PATH /c/msys64/usr/bin/make -j

CXX      = g++
WINDRES  = windres
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -municode -DUNICODE -D_UNICODE
LDFLAGS  = -mwindows -municode -static
LDLIBS   = -ld2d1 -ldwrite -lwindowscodecs -ldwmapi -lgdiplus \
           -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lgdi32 -luser32

TARGET   = daemon.scr
RES      = build/daemon.res
SRCDIR   = src
OBJDIR   = build/obj
BUILDTMP = build/tmp

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Keep compiler/linker temp files inside the project; don't depend on TMP/TEMP.
export TMP    := $(abspath $(BUILDTMP))
export TEMP   := $(TMP)
export TMPDIR := $(TMP)

.PHONY: all clean install run

all: $(TARGET)

$(OBJDIR) $(BUILDTMP):
	mkdir -p $@

$(RES): daemon.rc daemon.manifest docs/daemon.script | $(BUILDTMP)
	mkdir -p $(dir $@)
	$(WINDRES) daemon.rc -O coff -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJS) $(RES)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(RES) $(LDFLAGS) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) /w

clean:
	rm -rf $(TARGET) build

install: $(TARGET)
	cp $(TARGET) "$(SYSTEMROOT)/System32/"

-include $(DEPS)
