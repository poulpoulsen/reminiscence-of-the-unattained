# RACK_DIR muss auf das entpackte Rack-SDK zeigen, z.B. via:
#   export RACK_DIR=~/vcvrack-sdk/Rack-SDK   (bash)
#   $env.RACK_DIR = ...                       (nushell, in config.nu)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
CFLAGS +=
CXXFLAGS +=

LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += LICENSE

include $(RACK_DIR)/plugin.mk
