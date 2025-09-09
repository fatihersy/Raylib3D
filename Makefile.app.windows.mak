DIR := $(subst /,\,${CURDIR})
BUILD_DIR := bin
VENDOR_DIR := vendor
OBJ_DIR := obj

TITLE := raylib3d
ASSEMBLY := app
EXTENSION := .exe
COMPILER_FLAGS := -g -MD -std=c++23 -Werror=vla -Wall -Wextra -Wpedantic -Wno-unused-function
INCLUDE_FLAGS := -Ivendor/include -Iapp/src
LINKER_FLAGS := -g -L$(OBJ_DIR)/ -L$(VENDOR_DIR)/lib/ -L$(BUILD_DIR) -lraylib -lucrtbase -lGdi32 -lWinMM -lUser32 -lShell32  #-Wl,-rpath,.
DEFINES := -D_DEBUG

# Make does not offer a recursive wildcard function, so here's one:
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

SRC_FILES := $(call rwildcard,$(ASSEMBLY)/,*.cpp) # Get all .cpp files
DIRECTORIES := \$(ASSEMBLY)\src $(subst $(DIR),,$(shell dir $(ASSEMBLY)\src /S /AD /B | findstr /i src)) # Get all directories under src.
OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o) # Get all compiled .cpp.o objects

all: scaffold compile link

.PHONY: scaffold
scaffold: # create build directory
	@echo Scaffolding folder structure...
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	@echo Done.

.PHONY: link
link: scaffold $(OBJ_FILES) # link
	@echo Linking $(ASSEMBLY)...
	@clang++ $(OBJ_FILES) -o $(BUILD_DIR)/$(TITLE)$(EXTENSION) $(LINKER_FLAGS)

.PHONY: compile
compile: #compile .cpp files
	@echo Compiling...

.PHONY: clean
clean: # clean build directory
	if exist $(BUILD_DIR)\$(TITLE)$(EXTENSION) del $(BUILD_DIR)\$(TITLE)$(EXTENSION)
	rmdir /s /q $(OBJ_DIR)\$(ASSEMBLY)

$(OBJ_DIR)/%.cpp.o: %.cpp # compile .cpp to .cpp.o object
	@echo   $<...
	@clang++ $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)
	