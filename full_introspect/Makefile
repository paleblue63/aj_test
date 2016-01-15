# Copyright AllSeen Alliance. All rights reserved.
#
#    Permission to use, copy, modify, and/or distribute this software for any
#    purpose with or without fee is hereby granted, provided that the above
#    copyright notice and this permission notice appear in all copies.
#
#    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# returns current working directory
TOP := $(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

ALLJOYN_DIST := ../..
# for use by AllJoyn developers. Code built from alljoyn_core
#ALLJOYN_DIST := ../../build/linux/x86/debug/dist
#ALLJOYN_DIST := ../../build/linux/x86/release/dist
#ALLJOYN_DIST := ../../build/linux/x86_64/debug/dist
#ALLJOYN_DIST := ../../build/linux/x86_64/release/dist
# for use by AllJoyn developers. Code built from master
#ALLJOYN_DIST := ../../../build/linux/x86/debug/dist
#ALLJOYN_DIST := ../../../build/linux/x86/release/dist
#ALLJOYN_DIST := ../../../build/linux/x86_64/debug/dist
#ALLJOYN_DIST := ../../../build/linux/x86_64/release/dist

OBJ_DIR := obj

BIN_DIR := bin

ALLJOYN_LIB := $(ALLJOYN_DIST)/lib/liballjoyn.a

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX

LIBS = -lstdc++ -lcrypto -lpthread -lrt

.PHONY: default clean

default: all

all: light_controller

light_controller: light_controller.o $(ALLJOYN_LIB)
	mkdir -p $(BIN_DIR)
	$(CXX) -o $(BIN_DIR)/$@ $(OBJ_DIR)/light_controller.o $(TOP)$(ALLJOYN_LIB) $(LIBS)

light_controller.o: light_controller.cc $(ALLJOYN_LIB)
	mkdir -p $(OBJ_DIR)
	$(CXX) -c $(CXXFLAGS) -I$(ALLJOYN_DIST)/inc -o $(OBJ_DIR)/$@ light_controller.cc

clean: clean_light_controller
	rmdir $(OBJ_DIR)
	rmdir $(BIN_DIR)

clean_light_controller:
	rm -f $(OBJ_DIR)/light_controller.o $(BIN_DIR)/light_controller

