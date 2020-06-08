OS        := $(shell uname)
BASEDIR    = ..
GNUDIR     = $(BASEDIR)/gnu
CC          = gcc
CCC         = g++
ARCHIVER    = ar
ARCHTYPE    = 
VERSION = 0

SCARAB_DIR ?= ../../
OBJDIR := obj
CCFILES = message_queue_interface_lib.cc pin_scarab_common_lib.cc
CFILES = uop_generator.c
OBJFILES = $(patsubst %.cc,$(OBJDIR)/%.o,$(CCFILES)) $(patsubst %.c,$(OBJDIR)/%.o,$(CFILES))
TARGET = libpin_fe

# will always generate the debug functions, but won't call them unless debug is enabled
DEBUG_FLAGS = #-DNO_DEBUG
XFLAGS	    = -D_REENTRANT
WARN_CFLAGS = -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -Wno-long-long -Wpointer-arith -Wimplicit -Werror
ifeq ($(CCVER), gcc (GCC) 4.7.3)
WARN_CFLAGS += -Wno-unused-but-set-variable -Wno-maybe-uninitialized
endif
WARN_CCFLAGS= -Wall -Wunused -Wno-long-long -Wpointer-arith -Werror

NO_STAT ?=
PIN_COMPILE ?=
INCLUDES = -I$(SCARAB_DIR)
OPTCFLAGS   = -std=gnu99 -O3 $(ARCHTYPE) -funroll-loops -Werror $(INCLUDES) $(DEBUG_FLAGS) $(WARN_CFLAGS) $(XFLAGS) $(NO_STAT) $(PIN_COMPILE)# -DNO_ASSERT 
OPTCCFLAGS   = -std=c++14 -O3 $(ARCHTYPE) -fno-inline -funroll-loops -Werror $(INCLUDES) $(DEBUG_FLAGS) $(WARN_CCFLAGS) $(XFLAGS) $(NO_STAT) $(PIN_COMPILE)# -DNO_ASSERT 

DEPDIR := $(OBJDIR)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

.PHONY : clean dir all dbg objects archive

all:
	make dir
	make archive

$(OBJDIR)/$(TARGET).a: $(OBJFILES)
	$(ARCHIVER) rcs $@ $^

$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(OPTCFLAGS) -fPIC -c -o $@ $<

$(OBJDIR)/%.o: %.cc $(DEPDIR)/%.d
	$(CCC) $(DEPFLAGS) $(OPTCCFLAGS) -fPIC -c -o $@ $<

dbg: OPTCCFLAGS += -g -DDEBUG
dbg: all

vgr: OPTCFLAGS = -std=gnu99 -O0 -g $(ARCHTYPE) -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CFLAGS) $(XFLAGS) $(NO_STAT) $(PIN_COMPILE)# -DNO_ASSERT 
vgr: OPTCCFLAGS = -std=c++14 -O0 -g $(ARCHTYPE) -fno-inline -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CCFLAGS) $(XFLAGS) $(NO_STAT) $(PIN_COMPILE)
vgr: |dir archive

objects: $(OBJFILES)

archive: $(OBJDIR)/$(TARGET).a

dir:
	mkdir -p $(OBJDIR)

clean:
	-rm -rf $(OBJDIR)

DEPFILES := $(OBJFILES:$(OBJDIR)/%.o=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))
