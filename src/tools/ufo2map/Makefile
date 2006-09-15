# =================================
# Map Compiler
# =================================

# =================================
# Directories and Targets
# =================================

src_bsptools?=$(src_tools)/ufo2map
build_bsptools?=$(build_tools)/bsptools
mkdir_dir_list+=$(build_bsptools)

target_ufo2map?=$(DIR_BUILD)/ufo2map
target_bspinfo?=$(DIR_BUILD)/bspinfo

# =================================
# Includes
# =================================

include $(src_tools)/ufo2map/common/Makefile

# =================================
# Objects
# =================================

obj_ufo2map+=$(build_bsptools)/brushbsp.o
obj_ufo2map+=$(build_bsptools)/csg.o
obj_ufo2map+=$(build_bsptools)/faces.o
obj_ufo2map+=$(build_bsptools)/glfile.o
obj_ufo2map+=$(build_bsptools)/levels.o
obj_ufo2map+=$(build_bsptools)/lightmap.o
obj_ufo2map+=$(build_bsptools)/map.o
obj_ufo2map+=$(build_bsptools)/nodraw.o
obj_ufo2map+=$(build_bsptools)/patches.o
obj_ufo2map+=$(build_bsptools)/portals.o
obj_ufo2map+=$(build_bsptools)/qbsp3.o
obj_ufo2map+=$(build_bsptools)/qrad3.o
obj_ufo2map+=$(build_bsptools)/routing.o
obj_ufo2map+=$(build_bsptools)/textures.o
obj_ufo2map+=$(build_bsptools)/tree.o
obj_ufo2map+=$(build_bsptools)/ufo2map.o
obj_ufo2map+=$(build_bsptools)/writebsp.o

obj_bspinfo+=$(build_bsptools)/bspinfo3.o

# =================================
# Implicit Rules
# =================================

$(build_bsptools)/%.o: $(src_bsptools)/%.c $(src_bsptools)/%.h
	$(CC_TOOLS)

$(build_bsptools)/%.o: $(src_bsptools)/%.c
	$(CC_TOOLS)

# =================================
# Target-specific Rules
# =================================

.PHONY: ufo2map bspinfo

ufo2map: mkdirs $(target_ufo2map)

bspinfo: mkdirs $(target_bspinfo)

$(target_ufo2map): $(obj_ufo2map) $(obj_bsptools_common)
	@echo " * [LINK]   $@"; $(CC) $(CFLAGS) $(CFLAGS_TOOLS) $(LDLIBS) $(obj_bsptools_common) $(obj_ufo2map) -o $(target_ufo2map)

$(target_bspinfo): $(obj_bspinfo) $(obj_bsptools_common)
	@echo " * [LINK]   $@"; $(CC) $(CFLAGS) $(CFLAGS_TOOLS) $(LDLIBS) $(obj_bsptools_common) $(obj_bspinfo) -o $(target_bspinfo)
