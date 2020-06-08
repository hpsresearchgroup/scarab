#--------------------------------------------------------------------------------
#
# Commands:
#
#     make opt	     = Build optimized executable
#     make dbg	     = Build debugging executable
#     make gpf	     = Build gprof-capable executable
#     make vgr	     = Build valgrind-capable executable
#     make all	     = Build all executables
#     make clean     = Clean *all* executables, objects, dependencies, etc.
#     make cleandep  = Clean all the dependency files
#     make cleanopt  = Clean the optimized build
#     make cleandbg  = Clean the debugging build
#     make cleangpf  = Clean the profiling build
#     make cleanvgr  = Clean the valgrind build
#     make etags     = Generate a TAGS file for the source 
#     make doxygen   = Generate doxygen documentation
#
#--------------------------------------------------------------------------------

# **************************************************************************************
# Paths

OS        := $(shell uname)
BASEDIR    = ..
GNUDIR     = $(BASEDIR)/gnu

OBJBASE    = ./obj/$(OS)
DEPBASE    = ./dep/$(OS)
BINBASE    = ./bin/$(OS)
EXE        = scarab
EXEFILE    = $(BASEDIR)/bin/$(OS)/$(EXE)
GTEST_DIR  = test

OPTOBJDIR  = $(OBJBASE)/opt
OPTDEPDIR  = $(DEPBASE)/opt
OPTBINDIR  = $(BINBASE)/opt
OPTEXE     = $(OPTBINDIR)/$(EXE)

DBGOBJDIR  = $(OBJBASE)/dbg
DBGDEPDIR  = $(DEPBASE)/dbg
DBGBINDIR  = $(BINBASE)/dbg
DBGEXE     = $(DBGBINDIR)/$(EXE)

GPFOBJDIR  = $(OBJBASE)/gpf
GPFDEPDIR  = $(DEPBASE)/gpf
GPFBINDIR  = $(BINBASE)/gpf
GPFEXE     = $(GPFBINDIR)/$(EXE)

GTEOBJDIR  = $(OBJBASE)/gte
GTEDEPDIR  = $(DEPBASE)/gte
GTEBINDIR  = $(BINBASE)/gte
GTEEXE     = $(GTEBINDIR)/$(EXE)

VGROBJDIR  = $(OBJBASE)/vgr
VGRDEPDIR  = $(DEPBASE)/vgr
VGRBINDIR  = $(BINBASE)/vgr
VGREXE     = $(VGRBINDIR)/$(EXE)

DEPERR     = /tmp/makedeperr.$(USER)
LOCALLINK  = $(EXE)

# **************************************************************************************
# Mercurial revision -- spruett: switched form HG to git
GITREV:= $(shell git rev-parse --short HEAD)

# **************************************************************************************
# Compile options

CC          = gcc
CCC         = g++
ARCHTYPE    = 
CCVER       = $(shell $(CC) --version | head -n 1)
XFLAGS	    = -D_REENTRANT
WARN_CFLAGS = -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -Wno-long-long -Wpointer-arith -Wimplicit -Werror --param max-vartrack-size=0
# these warning switches are conditional to stay compatible with common HPS gcc 4.1.2
ifeq ($(CCVER), gcc (GCC) 4.7.3)
WARN_CFLAGS += -Wno-unused-but-set-variable -Wno-maybe-uninitialized
endif
WARN_CCFLAGS= -Wall -Wunused -Wno-long-long -Wpointer-arith -Werror --param max-vartrack-size=0

DEBUG_FLAGS = -DNO_DEBUG

# Multi2sim requires pthreads. We need -static to deal with missing
# libraries on pool machines. Combining -pthread and -static, however,
# is an issue (see https://bugzilla.redhat.com/show_bug.cgi?id=115157
# for example). The "whole-archive" linker option fix is from there as
# well. Also setting -D_REENTRANT since -pthread implies it.
# -- spruett: I removed the liberty library from below command.
# --  used to be: LIBS	    = -lm -liberty -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lz
LIBS	    = -lm -lpin_fe -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lz -lramulator
CXXFLAGS    = -I$(GNUDIR)/$(OS)/include -I.
OPTLDFLAGS  = -L$(GNUDIR)/$(OS)/lib -L./pin/pin_lib/obj -L./ramulator/ -rdynamic
DBGLDFLAGS  = -L$(GNUDIR)/$(OS)/lib -L./pin/pin_lib/obj -L./ramulator/ -rdynamic
GPFLDFLAGS  = -pg -L$(GNUDIR)/$(OS)/lib -L$(BASEDIR)/lib -L./pin/pin_lib/obj -L./ramulator/ -rdynamic
GTELDFLAGS  = -L$(GNUDIR)/$(OS)/lib -L./pin/pin_lib/obj -lgtest -L./ramulator -rdynamic

# Note absence of -static: valgrind requires dynamic compilation to detect leaks
# See http://valgrind.org/docs/manual/faq.html for details
VGRLDFLAGS  = -L$(GNUDIR)/$(OS)/lib -L./pin/pin_lib/obj -L./ramulator/ -rdynamic

#OPTCFLAGS  = -O3 -funroll-loops -Werror $(WARN_FLAGS) $(XFLAGS) # -DNO_ASSERT 
OPTCFLAGS   = -std=gnu99 -O3 -g3 $(ARCHTYPE) -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CFLAGS) $(XFLAGS) # -DNO_ASSERT 
DBGCFLAGS   = -std=gnu99 -O0 -g3 $(WARN_CFLAGS) $(XFLAGS)
GPFCFLAGS   = -std=gnu99 $(OPTCFLAGS) -pg -g3
VGRCFLAGS   = -std=gnu99 -O0 -g3 -funroll-loops $(XFLAGS)
GTECFLAGS   = -std=gnu99 -O3 -g3 $(ARCHTYPE) -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CFLAGS) $(XFLAGS) # -DNO_ASSERT 

OPTCCFLAGS   = -std=c++14 -O3 -g3 $(ARCHTYPE) -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CCFLAGS) $(XFLAGS) # -DNO_ASSERT 
DBGCCFLAGS   = -std=c++14 -O0 -g3 $(WARN_CCFLAGS) $(XFLAGS)
GPFCCFLAGS   = -std=c++14 $(OPTCCFLAGS) -pg -g3
VGRCCFLAGS   = -std=c++14 -O0 -g3 -funroll-loops $(XFLAGS)
GTECCFLAGS   = -std=c++14 -O3 -g3 $(ARCHTYPE) -fno-inline -funroll-loops -Werror $(DEBUG_FLAGS) $(WARN_CCFLAGS) $(XFLAGS) # -DNO_ASSERT 


# **************************************************************************************
# Default commands

SHELL	     = /bin/bash
STRIP	     = /usr/bin/strip
RM	     = /bin/rm -f
CP	     = /bin/cp -f
LN	     = /bin/ln -sf
MKDIR	     = /bin/mkdir -p
ifeq ($(OS), Linux)
ECHO	     = echo -e
ifeq ($(ECHO), -e)
ECHO	     = echo
endif
else
ECHO	     = echo
endif
DATE         = /bin/date

RUN_SCRIPT   = $(BASEDIR)/bin/run_cmp_scarab
RUN_BATCH    = $(BASEDIR)/bin/run_cmp_sbatch
BENCH	     = gcc
SET	     = std
SARGS	     = # for command line compile running
SCARAB_ARGS  = +outdir . $(SARGS)
#SCARAB_ARGS = +outdir . $(SARGS) --track_br_file_in ../input/%bench-%input-top1000brmiss.in
#SCARAB_ARGS = +outdir . $(SARGS) --track_br_file_in ../input/%bench-%input-top1000brpathmiss,pid1,cf16.in

SRCPWD      := $(shell $(SHELL) -c 'pwd')
BASEPWD     := $(shell $(SHELL) -c '(cd $(BASEDIR) ; pwd | sed s/s/s/ | sed s/\\//\\\\\\\\\\\\\\\\\\\\\\\\\\//g)')
RELWD       := ../..$(shell $(SHELL) -c "$(ECHO) $(SRCPWD) | sed s/$(BASEPWD)//")

define make_dir_tree
	$(MKDIR) $(1);
	$(MKDIR) $(foreach d,$(SRC_DIRS),$(1)/$d)
endef


# **************************************************************************************
# Generate all of the file lists

SRC_DIRS := globals isa libs frontend debug power dvfs prefetcher bp memory

CSRC_BASE:= $(wildcard *.c) $(foreach d, $(SRC_DIRS), $(wildcard $d/*.c))
CSRC     := $(CSRC_BASE)
GTEST_CSRC = $(filter-out main.c, $(CSRC_BASE))

CCSRC    := $(wildcard *.cc) $(foreach d, $(SRC_DIRS), $(wildcard $d/*.cc))
GTEST_CCSRC   = $(CCSRC) $(wildcard $(GTEST_DIR)/*.cc)

SRC      := $(patsubst %.c,%,$(CSRC)) $(patsubst %.cc,%,$(CCSRC))
GTEST_SRC:= $(patsubst %.c,%,$(GTEST_CSRC)) $(patsubst %.cc,%,$(GTEST_CCSRC))
OPTOBJS	 := $(patsubst %,$(OPTOBJDIR)/%.o,$(SRC))
DBGOBJS	 := $(patsubst %,$(DBGOBJDIR)/%.o,$(SRC))
GPFOBJS	 := $(patsubst %,$(GPFOBJDIR)/%.o,$(SRC))
VGROBJS	 := $(patsubst %,$(VGROBJDIR)/%.o,$(SRC))
GTEOBJS	 := $(patsubst %,$(GTEOBJDIR)/%.o,$(GTEST_SRC))



# **************************************************************************************
# Compilation rules

default: opt
	@$(ECHO) "Usage:  make {opt | dbg | gte | gpf | vgr | all | clean | cleandep | cleanopt | cleandbg | cleangte | cleangpf | cleanvgr | etags | doxygen}"
	@$(ECHO) "Assumed 'make opt'..."

scarab:  default

all: dbg gte gpf vgr opt ## Build all executables (opt, dbg, gpf, vgr, gte)
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(OPTEXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'
	$(LN) $(RELWD)/$(OPTEXE) $(BASEDIR)/bin/$(OS)/scarab

# the optimized version
opt: $(OPTEXE) ## Build the optimized executable
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(OPTEXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'	
	$(LN) $(RELWD)/$(OPTEXE) $(BASEDIR)/bin/$(OS)/scarab
$(OPTEXE): pin_fe_lib_opt ramulator_lib $(OPTOBJS)	 
	$(CCC) $(OPTLDFLAGS) -o $(OPTEXE) $(OPTOBJS) $(LIBS)
	$(STRIP) $(OPTEXE)


# the debugging version
dbg: $(DBGEXE) ## Build the debugging executable
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(DBGEXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'
	$(LN) $(RELWD)/$(DBGEXE) $(BASEDIR)/bin/$(OS)/scarab
$(DBGEXE): pin_fe_lib_dbg ramulator_lib_dbg $(DBGOBJS) 
	$(CCC) $(DBGLDFLAGS) -o $(DBGEXE) $(DBGOBJS) $(LIBS)

# the profiling version
gpf: $(GPFEXE) ## Build the gprof-capable executable
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(GPFEXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'
	$(LN) $(RELWD)/$(GPFEXE) $(BASEDIR)/bin/$(OS)/scarab
$(GPFEXE): pin_fe_lib_gpf ramulator_lib_gpf $(GPFOBJS)
	$(CCC) $(GPFLDFLAGS) -o $(GPFEXE) $(GPFOBJS) $(LIBS)

# the valgrind version
vgr: $(VGREXE) ## Build the valgrind-capable executable
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(VGREXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'
	$(LN) $(RELWD)/$(VGREXE) $(BASEDIR)/bin/$(OS)/scarab
$(VGREXE): pin_fe_lib_vgr ramulator_lib_vgr $(VGROBJS)
	$(CCC) $(VGRLDFLAGS) -o $(VGREXE) $(VGROBJS) $(VGRLIBS) $(LIBS) 

gtest: $(GTEEXE) ## Build all of the gtests
	@$(SHELL) -c 'if [ -L $(LOCALLINK) ]; then $(RM) $(LOCALLINK); fi'
	$(LN) $(GTEEXE) $(LOCALLINK)
	@$(SHELL) -c 'if [ -L $(EXEFILE) ]; then $(RM) $(EXEFILE); fi'
	$(LN) $(RELWD)/$(GTEEXE) $(BASEDIR)/bin/$(OS)/scarab
$(GTEEXE): pin_fe_lib_opt $(GTEOBJS)
	$(CCC) $(GTELDFLAGS) -o $(GTEEXE) $(GTEOBJS) $(LIBS)


# Ramulator
ramulator_lib:
	cd ramulator && make -j

ramulator_lib_dbg:
	cd ramulator && make -j dbg

ramulator_lib_gpf:
	cd ramulator && make -j gpf

ramulator_lib_vgr:
	cd ramulator && make -j

# PIN Exec Drive FE library
pin_fe_lib_opt:
	make SCARAB_DIR=$(SRCPWD) --directory pin/pin_lib
	make SCARAB_DIR=$(SRCPWD) pin_exec --directory pin/pin_exec

pin_fe_lib_dbg:
	make SCARAB_DIR=$(SRCPWD) --directory pin/pin_lib dbg
	make SCARAB_DIR=$(SRCPWD) pin_exec --directory pin/pin_exec

pin_fe_lib_gte:
	make SCARAB_DIR=$(SRCPWD) --directory pin/pin_lib
	make SCARAB_DIR=$(SRCPWD) pin_exec --directory pin/pin_exec

pin_fe_lib_gpf:
	make SCARAB_DIR=$(SRCPWD) --directory pin/pin_lib
	make SCARAB_DIR=$(SRCPWD) pin_exec --directory pin/pin_exec

pin_fe_lib_vgr:
	make SCARAB_DIR=$(SRCPWD) --directory pin/pin_lib vgr
	make SCARAB_DIR=$(SRCPWD) pin_exec --directory pin/pin_exec

clean_pin_fe_lib_opt:
	make --directory pin/pin_lib clean
	make --directory pin/pin_exec clean

clean_pin_fe_lib_dbg:
	make --directory pin/pin_lib clean
	make --directory pin/pin_exec clean

clean_pin_fe_lib_gte:
	make --directory pin/pin_lib clean
	make --directory pin/pin_exec clean

clean_pin_fe_lib_gpf:
	make --directory pin/pin_lib clean
	make --directory pin/pin_exec clean

clean_pin_fe_lib_vgr:
	make --directory pin/pin_lib clean

clean_ramulator_lib:
	cd ramulator && make clean

# Mercurial revision (double colon means "do always")
gitrev::
	[ -f $@ ] || touch $@
	echo '"'$(GITREV)'"' | cmp -s $@ - || echo '"'$(GITREV)'"' > $@

version.c: gitrev

clang-format: ## Run clang-format on the entire repository (not just src/*)
	../bin/run_clang_format_on_all.sh

release: clang-format ## Run clang-format and compile opt, dbg, vgr, and gpf. Should be run to ensure Scarab is ready for release.
	make clean
	make -j opt
	make -j dbg
	make -j vgr
	make -j gpf
	@echo
	@echo "Ready for release!"

help: ## Print this message
	@echo "Scarab Makefile:"
	@echo
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

help-nocolor: ## Print this message
	@echo "# Scarab Makefile:"
	@echo
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "* **%-30s:**\t\t%s\n", $$1, $$2}'

# **************************************************************************************
# Cleaning Rules

cleanopt: clean_pin_fe_lib_opt clean_ramulator_lib ## Clean the optimized build
	@$(ECHO) "Cleaning optimized objects and dependencies..."
	@-$(RM) $(OPTOBJDIR)/*.o $(OPTCDEPS) $(OPTCCDEPS) $(OPTEXE) 2> /dev/null
	@-$(RM) -rf $(OPTOBJDIR) $(OPTDEPDIR) $(OPTBINDIR)
cleandbg: clean_pin_fe_lib_dbg clean_ramulator_lib ## Clean the debugging build
	@$(ECHO) "Cleaning debugging objects and dependencies..."
	@-$(RM) $(DBGOBJDIR)/*.o $(DBGCDEPS) $(DBGCCDEPS) $(DBGEXE) 2> /dev/null
	@-$(RM) -rf $(DBGOBJDIR) $(DBGDEPDIR) $(DBGBINDIR)
cleangte: clean_pin_fe_lib_gte clean_ramulator_lib ## Clean the gtests
	@$(ECHO) "Cleaning profiling objects and dependencies..."
	@-$(RM) $(GTEOBJDIR)/*.o $(GTECDEPS) $(GTECCDEPS) $(GTEEXE) 2> /dev/null
	@-$(RM) -rf $(GTEOBJDIR) $(GTEDEPDIR) $(GTEBINDIR)
cleangpf: clean_pin_fe_lib_gpf clean_ramulator_lib ## Clean the profiling build
	@$(ECHO) "Cleaning profiling objects and dependencies..."
	@-$(RM) $(GPFOBJDIR)/*.o $(GPFCDEPS) $(GPFCCDEPS) $(GPFEXE) 2> /dev/null
	@-$(RM) -rf $(GPFOBJDIR) $(GPFDEPDIR) $(GPFBINDIR)
cleanvgr: ## Clean the valgrind build
	@$(ECHO) "Cleaning valgrind objects and dependencies..."
	@-$(RM) $(VGROBJDIR)/*.o $(VGRCDEPS) $(VGRCCDEPS) $(VGREXE) 2> /dev/null
	@-$(RM) -rf $(VGROBJDIR) $(VGRDEPDIR) $(VGRBINDIR)

cleandep: ## Clean all of the dependency files
	@$(ECHO) "Cleaning all dependency files..."
	@-$(RM) $(OPTCDEPS) $(OPTCCDEPS) $(DBGCDEPS) $(DBGCCDEPS) $(GTECDEPS) $(GTECCDEPS) $(GPFCDEPS) $(GPFCCDEPS) $(VGRCDEPS) $(VGRCCDEPS) 2> /dev/null

cleanall: clean
clean:    cleanopt cleandbg cleangte cleangpf cleanvgr ## Clean *all* executables, objects, dependencies, etc.
	@-$(RM) core *.core $(LOCALLINK) 2> /dev/null


# **************************************************************************************
# Etags rule

etags: ## Generate a TAGS file for the source
	@etags --regex='/^[ \t]*[A-Z0-9_]+,/' \
	--regex='/^struct[ \t]+\([A-Za-z_0-9]+\)_struct[ \t]*/\1/' \
	--regex='/^typedef[ \t]+[A-Za-z_0-9]+_struct.*$/dontevermatchme/' \
	`/bin/ls *.[ch] | /bin/grep -v global_types` global_types.h \
	--regex='/DEF_PARAM[ \t]*([ \t]*[a-z0-9_]+[ \t]*,[ \t]*[A-Z0-9_]+/' --regex='/DEF_PARAM[ \t]*([ \t]*KNOB_[A-Za-z0-9_]+/' *.param.def \
	--regex='/DEF_STAT[ \t]*([ \t]*[ \t]*[A-Z0-9_]+/' *.stat.def
	@$(BASEDIR)/bin/enum_tags *.[ch] >> TAGS

# **************************************************************************************
# Documentation

MAKEFILE_DOCS := $(BASEDIR)/docs/autogen-scarab-makefile-docs.md

docs: doxygen $(MAKEFILE_DOCS) ## Generate all doc files for Scarab

$(MAKEFILE_DOCS): Makefile
	make help-nocolor > $@

doxygen: ## Generate doxygen documentation
	@doxygen Doxyfile

# **************************************************************************************
# Simulation rules

run:
	$(RUN_SCRIPT) $(BENCH) $(SET) $(SCARAB_ARGS)

WAIT_FOR_JOBS_OPTS=-progress

QUICK_RELEASE_TRACE_ARGS = $(SARGS) \
	++read_tr --fetch_off_path_ops 0 --heartbeat_interval 0 \
	--fast_forward 10000000 --warmup 10000000 --inst_limit 10000000 \
	++bin $(OPTEXE) ++params $(BASEDIR)/src/PARAMS.quick_release ++nomail ++queue high_priority

quick_release: $(OPTEXE)
	@$(ECHO) Currently broken! FIXME
#	@$(RM) -r $(BASEDIR)/results/qr-{mp,tr,m2s}
#	@$(ECHO) Submitting jobs...
#	@$(RUN_BATCH) qr-mp  qr-mp@ref  $(QUICK_RELEASE_M2S_ARGS) --inst_limit 500000  | awk '{print $$4}' >  qr_jobs.out
#	@$(RUN_BATCH) qr-tr  spec06@ref $(QUICK_RELEASE_TRACE_ARGS) | awk '{print $$4}' >> qr_jobs.out
#	@$(RUN_BATCH) qr-m2s spec06@ref $(QUICK_RELEASE_M2S_ARGS)   | awk '{print $$4}' >> qr_jobs.out
#	@$(BASEDIR)/bin/wait_for_jobs.pl $(WAIT_FOR_JOBS_OPTS) < qr_jobs.out
#	@$(SHELL) -c "grep Finished $(BASEDIR)/results/qr-{mp,m2s}/*/stdout.out | sed -e 's&--.*&&' -e 's&/stdout.out&&' | LC_ALL=C sort > quick_release.out";
#	@$(SHELL) -c "zgrep Finished $(BASEDIR)/results/qr-{mp,tr,m2s}/*/stdout.out.gz | sed -e 's&--.*&&' -e 's&/stdout.out.gz&&' | LC_ALL=C sort > quick_release.out";
#	@$(SHELL) -c 'diff quick_release.out quick_release.stats; if [ $$? == 0 ]; then echo PASSED; exit 0; else echo FAILED; exit 1;fi'

VALGRIND_TRACE_ARGS = $(SARGS) \
	++read_tr --fetch_off_path_ops 0 --heartbeat_interval 0 \
	--fast_forward 10000 --warmup 10000 --inst_limit 10000 \
	++bin $(VGREXE) ++valgrind ++params $(BASEDIR)/src/PARAMS.quick_release ++nomail ++queue desktop

valgrind: $(VGREXE)
	@$(ECHO) Currently broken! FIXME
#	@$(RM) -r $(BASEDIR)/results/vg-{tr,m2s}
#	@$(ECHO) Submitting jobs...
## Valgrind runs with trace frontend fail for reasons I cannot fathom (bug #22)
##	@$(RUN_BATCH) vg-tr  spec06@ref $(VALGRIND_TRACE_ARGS) | grep 'id.hps.utexas.edu' >  vg_jobs.out
#	@$(RUN_BATCH) vg-m2s spec06@ref $(VALGRIND_M2S_ARGS)   | grep 'id.hps.utexas.edu' >  vg_jobs.out
#	@$(BASEDIR)/bin/wait_for_jobs.pl $(WAIT_FOR_JOBS_OPTS) < vg_jobs.out
#	@$(SHELL) -c "grep -e 'ERROR SUMMARY:' $(BASEDIR)/results/vg-m2s/*/*ER > valgrind.out";
#	@$(SHELL) -c 'grep -v " 0 error" valgrind.out; if [ $$? == 1 ]; then echo PASSED; exit 0; else echo FAILED; exit 1; fi'

# **************************************************************************************
# Dependency file rules - Each file has the dependencies for the object file and for the
#			    dependency file itself.

# optimized dependency files
$(OPTDEPDIR)/%.d: %.c
	@$(ECHO) Building \'$@, $*\'
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(OPTOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CC) $(OPTCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(OPTOBJDIR)/$*.o -c $*.c" | cat >> $@
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(OPTDEPDIR)/$*.d,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(OPTDEPDIR)/%.dd: %.cc
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(OPTOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(OPTCCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(OPTOBJDIR)/$*.o -c $*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(OPTDEPDIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)

# debugging dependency files
$(DBGDEPDIR)/%.d: %.c
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(DBGOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CC) $(DBGCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(DBGOBJDIR)/$*.o -c $*.c" | cat >> $@
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(DBGDEPDIR)/$*.d,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(DBGDEPDIR)/%.dd: %.cc
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(DBGOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(DBGCCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(DBGOBJDIR)/$*.o -c $*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(DBGDEPDIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)

# gtest dependency files
$(GTEDEPDIR)/%.d: %.c
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(GTEOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CC) $(GTECFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(GTEOBJDIR)/$*.o -c $*.c" | cat >> $@
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(GTEDEPDIR)/$*.d,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(GTEDEPDIR)/$(GTEST_DIR)/%.dd: $(GTEST_DIR)/%.cc
	@$(ECHO) Building GTEST \'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(GTEOBJDIR)\/$(GTEST_DIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(GTECCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(GTEOBJDIR)/$(GTEST_DIR)/$*.o -c $(GTEST_DIR)/$*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(GTEDEPDIR)/$(GTEST_DIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(GTEDEPDIR)/%.dd: %.cc
	@$(ECHO) Building GTECC\'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(GTEOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(GTECCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(GTEOBJDIR)/$*.o -c $*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(GTEDEPDIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)

# profiling dependency files
$(GPFDEPDIR)/%.d: %.c
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(GPFOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CC) $(GPFCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(GPFOBJDIR)/$*.o -c $*.c" | cat >> $@
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(GPFDEPDIR)/$*.d,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(GPFDEPDIR)/%.dd: %.cc
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(GPFOBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(GPFCCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(GPFOBJDIR)/$*.o -c $*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(GPFDEPDIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)

# valgrind dependency files
$(VGRDEPDIR)/%.d: %.c
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(VGROBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CC) $(VGRCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(VGROBJDIR)/$*.o -c $*.c" | cat >> $@
	@$(SHELL) -c '$(CC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(VGRDEPDIR)/$*.d,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)
$(VGRDEPDIR)/%.dd: %.cc
	@$(ECHO) Building \'$@\'
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< 2> $(DEPERR) | sed '\''s,$(notdir $*)[.]o,$(VGROBJDIR)\/$*.o,g'\'' > $@' 
	@$(ECHO) "\t$(CCC) $(VGRCCFLAGS) $(CXXFLAGS) $(XFLAGS) -o $(VGROBJDIR)/$*.o -c $*.cc" | cat >> $@
	@$(SHELL) -c '$(CCC) $(CXXFLAGS) $(XFLAGS) -M $< | sed '\''s,$*[.]o,$(VGRDEPDIR)/$*.dd,g'\'' >> $@'
	@$(SHELL) -c 'if [ -s $(DEPERR) ]; then $(RM) $@; fi'
	@-$(RM) $(DEPERR)


# **************************************************************************************
# Include all of the dependency files and make sure the directory structure is intact

OPTCDEPS  := $(patsubst %.c,$(OPTDEPDIR)/%.d,$(CSRC))
DBGCDEPS  := $(patsubst %.c,$(DBGDEPDIR)/%.d,$(CSRC))
GTECDEPS  := $(patsubst %.c,$(GTEDEPDIR)/%.d,$(GTEST_CSRC))
GPFCDEPS  := $(patsubst %.c,$(GPFDEPDIR)/%.d,$(CSRC))
VGRCDEPS  := $(patsubst %.c,$(VGRDEPDIR)/%.d,$(CSRC))

OPTCCDEPS := $(patsubst %.cc,$(OPTDEPDIR)/%.dd,$(CCSRC))
DBGCCDEPS := $(patsubst %.cc,$(DBGDEPDIR)/%.dd,$(CCSRC))
GTECCDEPS := $(patsubst %.cc,$(GTEDEPDIR)/%.dd,$(GTEST_CCSRC))
GPFCCDEPS := $(patsubst %.cc,$(GPFDEPDIR)/%.dd,$(CCSRC))
VGRCCDEPS := $(patsubst %.cc,$(VGRDEPDIR)/%.dd,$(CCSRC))

ifeq ($(MAKECMDGOALS), all)
-include $(OPTCDEPS) $(OPTCCDEPS)
-include $(DBGCDEPS) $(DBGCCDEPS)
-include $(GTECDEPS) $(GTECCDEPS)
-include $(GPFCDEPS) $(GPFCCDEPS)
-include $(VGRCDEPS) $(VGRCCDEPS)
endif

ifeq ($(MAKECMDGOALS), both)
-include $(OPTCDEPS) $(OPTCCDEPS)
-include $(DBGCDEPS) $(DBGCCDEPS)
endif

ifeq ($(MAKECMDGOALS), gtest)
-include $(GTECDEPS) $(GTECCDEPS)
endif

ifeq ($(MAKECMDGOALS), )
-include $(OPTCDEPS) $(OPTCCDEPS)
endif

ifeq ($(MAKECMDGOALS), opt)
-include $(OPTCDEPS) $(OPTCCDEPS)
endif

ifeq ($(MAKECMDGOALS), dbg)
-include $(DBGCDEPS) $(DBGCCDEPS)
endif

ifeq ($(MAKECMDGOALS), gpf)
-include $(GPFCDEPS) $(GPFCCDEPS)
endif

ifeq ($(MAKECMDGOALS), vgr)
-include $(VGRCDEPS) $(VGRCCDEPS)
endif


# **************************************************************************************
# Build the directory structure

MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(OBJBASE) ]; then $(ECHO) "Making object tree..."; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(OBJBASE) ]; then $(MKDIR) $(OBJBASE); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(OPTOBJDIR) ]; then $(call make_dir_tree,$(OPTOBJDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(DBGOBJDIR) ]; then $(call make_dir_tree,$(DBGOBJDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEOBJDIR) ]; then $(call make_dir_tree,$(GTEOBJDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEOBJDIR)/test ]; then $(call make_dir_tree,$(GTEOBJDIR))/test; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GPFOBJDIR) ]; then $(call make_dir_tree,$(GPFOBJDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(VGROBJDIR) ]; then $(call make_dir_tree,$(VGROBJDIR)); fi')

MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(DEPBASE) ]; then $(ECHO) "Making dependency tree..."; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(DEPBASE) ]; then $(MKDIR) $(DEPBASE); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(OPTDEPDIR) ]; then $(call make_dir_tree,$(OPTDEPDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(DBGDEPDIR) ]; then $(call make_dir_tree,$(DBGDEPDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEDEPDIR) ]; then $(call make_dir_tree,$(GTEDEPDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEDEPDIR)/test ]; then $(call make_dir_tree,$(GTEDEPDIR))/test; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GPFDEPDIR) ]; then $(call make_dir_tree,$(GPFDEPDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(VGRDEPDIR) ]; then $(call make_dir_tree,$(VGRDEPDIR)); fi')

MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(BINBASE) ]; then $(ECHO) "Making binary tree..."; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(BINBASE) ]; then $(MKDIR) $(BINBASE); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(OPTBINDIR) ]; then $(call make_dir_tree,$(OPTBINDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(DBGBINDIR) ]; then $(call make_dir_tree,$(DBGBINDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEBINDIR) ]; then $(call make_dir_tree,$(GTEBINDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GTEBINDIR)/test ]; then $(call make_dir_tree,$(GTEBINDIR))/test; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(GPFBINDIR) ]; then $(call make_dir_tree,$(GPFBINDIR)); fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(VGRBINDIR) ]; then $(call make_dir_tree,$(VGRBINDIR)); fi')

MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(BASEDIR)/bin/$(OS) ]; then $(ECHO) "Making base binary tree..."; fi')
MAKEDIRS := $(shell $(SHELL) -c 'if [ ! -d $(BASEDIR)/bin/$(OS) ]; then $(MKDIR) $(BASEDIR)/bin/$(OS); fi')
