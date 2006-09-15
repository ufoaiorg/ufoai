# =================================
# Tools
# =================================

# =================================
# Directories and Targets
# =================================

src_tools?=$(src_root)/tools
build_tools?=$(DIR_BUILD)/tools
mkdir_dir_list+=$(build_tools)

# =================================
# Includes
# =================================

include $(src_tools)/ufo2map/Makefile

# =================================
# Implicit Rules
# =================================

# =================================
# Target-specific Rules
# =================================

.PHONY: tools

tools: ufo2map bspinfo
