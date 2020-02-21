CC=gcc
CCC=g++

C_WARN_FLAGS=-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -Wno-long-long -Wpointer-arith -Wimplicit -Werror
CC_WARN_FLAGS=-Wall -Wunused -Wmissing-declarations -Wno-long-long -Wpointer-arith -Werror

OPTCCFLAGS=--std=c++11 -O3 $(CC_WARN_FLAGS) -mxsave
OPTCFLAGS=-O3 $(C_WARN_FLAGS)

OBJDIR=obj
CCFILES=$(wildcard *.cc)
CFILES=$(wildcard *.c)

OFILES=$(patsubst %.cc,$(OBJDIR)/%.o,$(CCFILES)) $(patsubst %.c,$(OBJDIR)/%.o,$(CFILES))
CHECKPOINT_LOADER_OFILES=$(OFILES)

CHECKPOINT_LOADER_DEPS=$(CHECKPOINT_LOADER_OFILES:%.o=%.d)
DEP=$(CHECKPOINT_LOADER_DEPS)

CHECKPOINT_LOADER := checkpoint_loader

############################################################################
# Building Checkpoint Loader
############################################################################

.PHONY: all clean dir

all: $(CHECKPOINT_LOADER) $(PIN_EXEC_PATH)/$(CHK_PNT_PIN_TOOL)

clean:
	-rm -f $(CHECKPOINT_LOADER)
	-rm -rf $(OBJDIR)
	-make clean_checkpoint_pintool

dir: $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(CHECKPOINT_LOADER): $(CHECKPOINT_LOADER_OFILES)
	$(CCC) $(OPTCCFLAGS) -o $@ $^

-include $(DEP)

$(OBJDIR)/%.o: %.s $(OBJDIR)
	as -o $@ $<

$(OBJDIR)/%.o: %.c Makefile $(OBJDIR)
	$(CC) $(OPTCFLAGS) -MMD -fPIC -c -o $@ $<

$(OBJDIR)/%.o: %.cc Makefile $(OBJDIR)
	$(CCC) $(OPTCCFLAGS) -MMD -fPIC -c -o $@ $<

