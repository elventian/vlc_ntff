LD = ld
CC = g++
PKG_CONFIG = pkg-config
INSTALL = install
CXXFLAGS = -g -O2 -Wall -Wextra -std=c++1z
LDFLAGS =
LIBS =
VLC_PLUGIN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vlc-plugin)
VLC_PLUGIN_LIBS := $(shell $(PKG_CONFIG) --libs vlc-plugin)
VLC_PLUGIN_DIR := $(shell $(PKG_CONFIG) --variable=pluginsdir vlc-plugin)
 
plugindir = $(VLC_PLUGIN_DIR)/misc
 
override CC += -std=gnu11
override CPPFLAGS += -DPIC -I. -Isrc
override CXXFLAGS += -fPIC
override LDFLAGS += -Wl,-no-undefined,-z,defs
 
override CPPFLAGS += -DMODULE_STRING=\"ntff\"
override CXXFLAGS += $(VLC_PLUGIN_CFLAGS) -I/usr/src/vlc-3.0.8/include 
override LIBS += $(VLC_PLUGIN_LIBS) -lstdc++fs
 
all: libntff_plugin.so
 
install: all
	mkdir -p -- $(DESTDIR)$(plugindir)
	$(INSTALL) --mode 0755 libntff_plugin.so $(DESTDIR)$(plugindir)

install-strip:
	$(MAKE) install INSTALL="$(INSTALL) -s"

uninstall:
	rm -f $(plugindir)/libntff_plugin.so

clean:
	rm -f -- libntff_plugin.so src/*.o

mostlyclean: clean
 
SOURCES = ntff_main.cpp ntff_es.cpp ntff_project.cpp ntff_feature.cpp ntff_player.cpp
 
$(SOURCES:%.cpp=src/%.o): $(SOURCES:%.cpp=src/%.cpp)
 
libntff_plugin.so: $(SOURCES:%.cpp=src/%.o)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)
 
.PHONY: all install install-strip uninstall clean mostlyclean

