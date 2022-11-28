-include Makefile.inc

LIBRARY_NAME=d2x
BASE_DIR=$(shell pwd)
SRC_DIR=$(BASE_DIR)/src
SAMPLES_DIR=$(BASE_DIR)/samples
BUILD_DIR?=$(BASE_DIR)/build
INCLUDE_DIR=$(BASE_DIR)/include

INCLUDES=$(wildcard $(INCLUDE_DIR)/*.h) $(wildcard $(INCLUDE_DIR)/*/*.h) 


DEBUG ?= 0

# Config is ready, check if config is consistent
CHECK_CONFIG=1
ifeq ($(MAKECMDGOALS), compile-flags)
CHECK_CONFIG=0
endif
ifeq ($(MAKECMDGOALS), linker-flags)
CHECK_CONFIG=0
endif
ifeq ($(MAKECMDGOALS), gdb-command)
CHECK_CONFIG=0
endif

ifeq ($(CHECK_CONFIG), 1)
CONFIG_STR=DEBUG=$(DEBUG)
CONFIG_FILE=$(BUILD_DIR)/build.config
$(shell mkdir -p $(BUILD_DIR))
$(shell touch $(CONFIG_FILE))

ifneq ($(shell cat $(CONFIG_FILE)), $(CONFIG_STR))
$(warning Previous config and current config does not match! Rebuilding)
$(shell rm -rf $(BUILD_DIR))
$(shell mkdir -p $(BUILD_DIR))
$(shell echo $(CONFIG_STR) > $(CONFIG_FILE))
endif

endif

$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(BUILD_DIR)/samples)
$(shell mkdir -p $(BUILD_DIR)/runtime)

CFLAGS_INTERNAL=-std=c++11
CFLAGS=
LINKER_FLAGS=
INCLUDE_FLAGS=


ifeq ($(DEBUG),1)
CFLAGS+=-g
LINKER_FLAGS+=-l$(LIBRARY_NAME) -g
else
CFLAGS_INTERNAL+=-O3
LINKER_FLAGS+=-l$(LIBRARY_NAME)
endif


BUILDIT_CFLAGS=$(shell make --no-print-directory -C $(BUILDIT_DIR) compile-flags)
BUILDIT_LINK_FLAGS=$(shell make --no-print-directory -C $(BUILDIT_DIR) linker-flags)



CFLAGS_INTERNAL+=-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-declarations -Woverloaded-virtual -Wno-deprecated -Wdelete-non-virtual-dtor -Werror -Wno-vla 
INCLUDE_FLAGS=-I$(INCLUDE_DIR) $(BUILDIT_CFLAGS)
CFLAGS_INTERNAL+=-pedantic-errors

LINKER_FLAGS+=-L$(BUILD_DIR)/ $(BUILDIT_LINK_FLAGS)

SRC=$(wildcard $(SRC_DIR)/*.cpp)
SAMPLE_SRC=$(wildcard $(SAMPLES_DIR)/*.cpp)

OBJS=$(subst $(SRC_DIR),$(BUILD_DIR),$(SRC:.cpp=.o))
SAMPLE_OBJS=$(subst $(SAMPLES_DIR),$(BUILD_DIR)/samples,$(SAMPLE_SRC:.cpp=.o))

SAMPLES=$(subst $(SAMPLES_DIR),$(BUILD_DIR),$(SAMPLE_SRC:.cpp=))

LIBRARY_OBJS=$(OBJS) 
LIBRARY=$(BUILD_DIR)/lib$(LIBRARY_NAME).a

all: $(LIBRARY) executables

lib: $(LIBRARY)

.PRECIOUS: $(BUILD_DIR)/%.o $(BUILD_DIR)/samples/%.o

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(INCLUDES)
	$(CXX) $(CFLAGS_INTERNAL) $(CFLAGS) $< -o $@ $(INCLUDE_FLAGS) -c


$(BUILD_DIR)/samples/%.o: $(SAMPLES_DIR)/%.cpp $(INCLUDES)
	$(CXX) $(CFLAGS) $< -o $@ $(INCLUDE_FLAGS) -c -DBASE_DIR_X=$(BASE_DIR)

$(LIBRARY): $(LIBRARY_OBJS)
	ar rv $(LIBRARY) $(LIBRARY_OBJS)

$(BUILD_DIR)/sample%: $(BUILD_DIR)/samples/sample%.o $(LIBRARY)
	$(CXX) -o $@ $< $(LINKER_FLAGS)

.PHONY: executables
executables: $(SAMPLES)

clean:
	- rm -rf $(BUILD_DIR)

.PHONY: compile-flags linker-flags
compile-flags:
	@echo $(CFLAGS) $(INCLUDE_FLAGS)

linker-flags:
	@echo $(LINKER_FLAGS)
gdb-command:
	@echo gdb --command=$(BASE_DIR)/helpers/gdb/d2x-gdb.init
