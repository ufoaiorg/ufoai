# Ports

# ============================
# Determine system type -> system dir.
# ============================

# Indentation ugly, but necessary :(

ifeq ($(OSTYPE),Linux)
  src_dir_sys = linux
else
  ifeq ($(OSTYPE),Darwin)
    src_dir_sys = macosx
  else
    ifeq ($(OSTYPE),SunOS)
      src_dir_sys = solaris
    else
      ifeq ($(OSTYPE),Windows)
        src_dir_sys = win32
      else
        ifeq ($(OSTYPE),Cygwin)
          src_dir_sys = win32
        else
          ifeq ($(OSTYPE),Null)
            src_dir_sys = null
          else
            ifeq ($(OSTYPE),Irix)
              src_dir_sys = irix
            else
              ifeq ($(OSTYPE),FreeBSD)
                src_dir_sys = bsd
              else
                $(error OS not supported)
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

include $(src_ports)/$(src_dir_sys)/Makefile

