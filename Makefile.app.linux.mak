
BUILD_DIR := bin
VENDOR_DIR := vendor
OBJ_DIR := obj

TITLE := raylib3d
ASSEMBLY := app
EXTENSION := 
COMPILER_FLAGS := -g -MD -Werror=vla -Wall -Wextra -Wpedantic -std=c++23
INCLUDE_FLAGS := -I./app/src -I./vendor/include
LINKER_FLAGS := -L./$(OBJ_DIR)/ -L./$(VENDOR_DIR)/lib/ -L./$(BUILD_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
DEFINES := -D_DEBUG

# Make does not offer a recursive wildcard function, so here's one:
#rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

SRC_FILES := $(shell find $(ASSEMBLY) -name *.cpp)		# .cpp files
DIRECTORIES := $(shell find $(ASSEMBLY) -type d)		# directories with .h files
OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)		# compiled .o objects

all: scaffold compile link

.PHONY: scaffold
scaffold: # create build directory
	@echo Scaffolding folder structure...
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
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
	rm -rf $(BUILD_DIR)\$(ASSEMBLY)
	rm -rf $(OBJ_DIR)\$(ASSEMBLY)

$(OBJ_DIR)/%.cpp.o: %.cpp # compile .cpp to .o object
	@echo   $<...
	@clang++ $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

-include $(OBJ_FILES:.o=.d) e
