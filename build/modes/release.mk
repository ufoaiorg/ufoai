CFLAGS += -ffast-math -funroll-loops -D_FORTIFY_SOURCE=2 -DNDEBUG

ifeq ($(filter -O0 -O1 -O2 -O3 -O4 -Os -Ofast,$(CFLAGS)),) # If you use multiple -O options, with or without level numbers, the last such option is the one that is effective
  CFLAGS += -O2
endif

ifeq ($(TARGET_ARCH),i386)
  CFLAGS += -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
endif

ifeq ($(TARGET_ARCH),x86_64)
  CFLAGS += -fexpensive-optimizations -fno-strict-aliasing
endif

ifeq ($(SSE),1) # Not all -O options are working with sse
  ifneq ($(filter -O2 -O3 -O4 -Ofast,$(CFLAGS)),)
    CFLAGS += -O1 -fthread-jumps -falign-functions -falign-jumps -falign-loops
    CFLAGS += -falign-labels -fcaller-saves -fcrossjumping -fcse-skip-blocks
    CFLAGS += -fdelete-null-pointer-checks -fexpensive-optimizations -fgcse-lm
    CFLAGS += -foptimize-sibling-calls -fpeephole2 -fregmove -freorder-blocks
    CFLAGS += -freorder-functions -frerun-cse-after-loop -fsched-interblock
    CFLAGS += -fsched-spec -fschedule-insns2 -fstrict-overflow -ftree-pre -ftree-vrp
  endif
  ifneq ($(filter -O3 -O4 -Ofast,$(CFLAGS)),)
    CFLAGS += -finline-functions -funswitch-loops -fpredictive-commoning
    CFLAGS += -fgcse-after-reload -ftree-vectorize -fno-strict-aliasing
  endif
  CFLAGS := -msse -mfpmath=sse $(filter-out -O2 -O3 -O4 -Ofast -funroll-loops,$(CFLAGS))
endif
