TARGET             := uforadiant-brushexport
RADIANT_BASE       := tools/radiant

$(TARGET)_FILE     := radiant/plugins/brushexport.$(SO_EXT)
$(TARGET)_CFLAGS   += $(SO_CFLAGS) -Isrc/$(RADIANT_BASE)/libs -Isrc/$(RADIANT_BASE)/include $(GTK_CFLAGS) $(GLIB_CFLAGS)
$(TARGET)_LDFLAGS  += $(SO_LDFLAGS) $(GTK_LIBS) $(GLIB_LIBS)
$(TARGET)_SRCS     := \
	$(RADIANT_BASE)/plugins/brushexport/callbacks.cpp \
	$(RADIANT_BASE)/plugins/brushexport/export.cpp \
	$(RADIANT_BASE)/plugins/brushexport/interface.cpp \
	$(RADIANT_BASE)/plugins/brushexport/plugin.cpp \
	$(RADIANT_BASE)/plugins/brushexport/support.cpp

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
