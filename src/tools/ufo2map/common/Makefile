# =================================
# Map Compiler Common
# =================================

# =================================
# Directories
# =================================

src_bsptools_common?=$(src_bsptools)/common
build_bsptools_common?=$(build_bsptools)/common
mkdir_dir_list+=$(build_bsptools_common)

# =================================
# Objects
# =================================

obj_bsptools_common+=$(build_bsptools_common)/bspfile.o
obj_bsptools_common+=$(build_bsptools_common)/cmdlib.o
obj_bsptools_common+=$(build_bsptools_common)/l3dslib.o
obj_bsptools_common+=$(build_bsptools_common)/lbmlib.o
obj_bsptools_common+=$(build_bsptools_common)/mathlib.o
obj_bsptools_common+=$(build_bsptools_common)/polylib.o
obj_bsptools_common+=$(build_bsptools_common)/scriplib.o
obj_bsptools_common+=$(build_bsptools_common)/threads.o
obj_bsptools_common+=$(build_bsptools_common)/trace.o
obj_bsptools_common+=$(build_bsptools_common)/trilib.o

# =================================
# Implicit Rules
# =================================

$(build_bsptools_common)/%.o: $(src_bsptools_common)/%.c $(src_bsptools_common)/%.h
	$(CC_TOOLS)

$(build_bsptools_common)/%.o: $(src_bsptools_common)/%.c
	$(CC_TOOLS)

# =================================
# Target-specific Rules
# =================================
