# Makefile for Remote Desktop Application

CXX = cl
CXXFLAGS = /std:c++14 /EHsc /W3 /O2
LIBS = ws2_32.lib user32.lib gdi32.lib comctl32.lib kernel32.lib
TARGET = RemoteDesktop.exe
SOURCE = RemoteDesktop.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) /link $(LIBS) /out:$(TARGET)

clean:
	del *.obj *.exe *.pdb 2>nul

.PHONY: all clean
