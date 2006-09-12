ifeq ($(MAKELEVEL,0))
  $(error Please run make from the src directory.)
endif

builddocs?=$(ROOTDIR)/docs

.DEFAULT:
	@echo "Invalid target specified. Please run make from src directory."

.PHONY: all clean

all:
	doxygen doxyall
	
clean:
	-@rm -Rf $(ROOTDIR)/docs
