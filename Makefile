-include Makefile.inc

LIBRARY_NAME=xray
BASE_DIR=$(shell pwd)
SRC_DIR=$(BASE_DIR)/src
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


CFLAGS_INTERNAL+=-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-declarations -Woverloaded-virtual -Wno-deprecated -Wdelete-non-virtual-dtor -Werror -Wno-vla 
INCLUDE_FLAGS=-I$(INCLUDE_DIR) 
CFLAGS_INTERNAL+=-pedantic-errors

LINKER_FLAGS+=-L$(BUILD_DIR)/

SRC=$(wildcard $(SRC_DIR)/*.cpp)

OBJS=$(subst $(SRC_DIR),$(BUILD_DIR),$(SRC:.cpp=.o))

LIBRARY_OBJS=$(OBJS) 
LIBRARY=$(BUILD_DIR)/$(LIBRARY_NAME).a

all: $(LIBRARY)

.PRECIOUS: $(BUILD_DIR)/%.o 

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(INCLUDES)
	$(CXX) $(CFLAGS_INTERNAL) $(CFLAGS) $< -o $@ $(INCLUDE_FLAGS) -c


$(LIBRARY): $(LIBRARY_OBJS)
	ar rv $(LIBRARY) $(LIBRARY_OBJS)


clean:
	- rm -rf $(BUILD_DIR)

.PHONY: compile-flags linker-flags
compile-flags:
	@echo $(CFLAGS) $(INCLUDE_FLAGS)

linker-flags:
	@echo $(LINKER_FLAGS)
