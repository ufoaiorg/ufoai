CFLAGS += -fno-inline

ifeq ($(filter -O0 -O1 -O2 -O3 -O4 -Os -Ofast,$(CFLAGS)),) # If you use multiple -O options, with or without level numbers, the last such option is the one that is effective
  CFLAGS += -O0
endif
