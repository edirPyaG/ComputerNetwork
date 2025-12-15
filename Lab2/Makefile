# RDT Protocol Implementation - Lab2
# Auto-detect Windows Architecture and Build

# Detect Architecture
!IF "$(PROCESSOR_ARCHITECTURE)" == "ARM64"
ARCH = ARM64
TARGET_ARCH = ARM64
!ELSE IF "$(PROCESSOR_ARCHITECTURE)" == "AMD64"
ARCH = x64
TARGET_ARCH = x64
!ELSE
ARCH = x86
TARGET_ARCH = x86
!ENDIF

# Compiler Settings
CC = cl
CFLAGS = /std:c++14 /EHsc /W3 /O2 /nologo /D_CRT_SECURE_NO_WARNINGS
LDFLAGS = /link ws2_32.lib

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj\$(ARCH)
BIN_DIR = bin\$(ARCH)

# Source Files
SENDER_SRC = $(SRC_DIR)\sender.cpp $(SRC_DIR)\config.cpp
RECEIVER_SRC = $(SRC_DIR)\receiver.cpp $(SRC_DIR)\config.cpp

# Object Files
SENDER_OBJ = $(OBJ_DIR)\sender.obj $(OBJ_DIR)\config.obj
RECEIVER_OBJ = $(OBJ_DIR)\receiver.obj $(OBJ_DIR)\config.obj

# Targets
SENDER_EXE = $(BIN_DIR)\sender.exe
RECEIVER_EXE = $(BIN_DIR)\receiver.exe

# Default Target
all: info $(OBJ_DIR) $(BIN_DIR) $(SENDER_EXE) $(RECEIVER_EXE)

info:
	@echo ========================================
	@echo Building RDT Protocol for $(ARCH)
	@echo ========================================

# Create Directories (safe even if they already exist)
$(OBJ_DIR):
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"

$(BIN_DIR):
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"

# Build Sender
$(SENDER_EXE): $(SENDER_OBJ) $(BIN_DIR)
	@echo Linking sender.exe for $(ARCH)...
	$(CC) $(SENDER_OBJ) /Fe:$(SENDER_EXE) $(LDFLAGS)
	@echo Sender built successfully: $(SENDER_EXE)

# Build Receiver
$(RECEIVER_EXE): $(RECEIVER_OBJ) $(BIN_DIR)
	@echo Linking receiver.exe for $(ARCH)...
	$(CC) $(RECEIVER_OBJ) /Fe:$(RECEIVER_EXE) $(LDFLAGS)
	@echo Receiver built successfully: $(RECEIVER_EXE)

# Compile Source Files
$(OBJ_DIR)\sender.obj: $(SRC_DIR)\sender.cpp $(INC_DIR)\rdt_protocol.h $(OBJ_DIR)
	@echo Compiling sender.cpp for $(ARCH)...
	$(CC) $(CFLAGS) /I$(INC_DIR) /c $(SRC_DIR)\sender.cpp /Fo:$(OBJ_DIR)\sender.obj

$(OBJ_DIR)\receiver.obj: $(SRC_DIR)\receiver.cpp $(INC_DIR)\rdt_protocol.h $(OBJ_DIR)
	@echo Compiling receiver.cpp for $(ARCH)...
	$(CC) $(CFLAGS) /I$(INC_DIR) /c $(SRC_DIR)\receiver.cpp /Fo:$(OBJ_DIR)\receiver.obj

$(OBJ_DIR)\config.obj: $(SRC_DIR)\config.cpp $(INC_DIR)\rdt_protocol.h $(OBJ_DIR)
	@echo Compiling config.cpp for $(ARCH)...
	$(CC) $(CFLAGS) /I$(INC_DIR) /c $(SRC_DIR)\config.cpp /Fo:$(OBJ_DIR)\config.obj

# Clean Build Artifacts
clean:
	@echo Cleaning build artifacts...
	@if exist "obj" rmdir /S /Q obj
	@if exist "bin" rmdir /S /Q bin
	@if exist "*.obj" del /Q *.obj
	@if exist "*.exe" del /Q *.exe
	@echo Clean complete.

# Clean specific architecture
clean-arch:
	@echo Cleaning $(ARCH) build artifacts...
	@if exist "$(OBJ_DIR)" rmdir /S /Q "$(OBJ_DIR)"
	@if exist "$(BIN_DIR)" rmdir /S /Q "$(BIN_DIR)"
	@echo Clean complete for $(ARCH).

# Rebuild
rebuild: clean all

# Help
help:
	@echo.
	@echo RDT Protocol Makefile - Usage:
	@echo   nmake              - Build for current architecture ($(ARCH))
	@echo   nmake clean        - Remove all build artifacts
	@echo   nmake clean-arch   - Remove artifacts for current arch
	@echo   nmake rebuild      - Clean and rebuild
	@echo   nmake help         - Show this help
	@echo.
	@echo Detected Architecture: $(ARCH)
	@echo.

.PHONY: all info clean clean-arch rebuild help