SRCDIR := ./
OBJDIR := obj
OBJDIR_DBG := obj_dbg
OBJDIR_GPF := obj_gpf

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
DBG_OBJS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR_DBG)/%.o, $(SRCS))
GPF_OBJS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR_GPF)/%.o, $(SRCS))

LIB_NAME := libramulator.a

# Ramulator currently supports g++ 5.1+ or clang++ 3.4+.  It will NOT work with
#   g++ 4.x due to an internal compiler error when processing lambda functions.
CXX := g++
AR := ar
#CXX := g++-5
CXXFLAGS := -O3 -std=c++14 -g3 -Wall
CXXFLAGS_DBG := -O0 -std=c++14 -g3 -Wall
CXXFLAGS_GPF := -O3 -std=c++14 -pg -Wall

.PHONY: all dbg gpf clean depend depend_dbg depend_gpf

all: depend lib_opt
	ln -sf lib_opt $(LIB_NAME)

dbg: depend lib_dbg
	ln -sf lib_dbg $(LIB_NAME)

gpf: depend lib_gpf
	ln -sf lib_gpf $(LIB_NAME)

clean:
	rm -f lib_opt
	rm -f lib_dbg
	rm -f lib_gpf
	rm -f libramulator.a
	rm -rf $(OBJDIR)
	rm -rf $(OBJDIR_DBG)
	rm -rf $(OBJDIR_GPF)

depend: $(OBJDIR)/.depend


$(OBJDIR)/.depend: $(SRCS)
	@mkdir -p $(OBJDIR)
	@rm -f $(OBJDIR)/.depend
	@$(foreach SRC, $(SRCS), $(CXX) $(CXXFLAGS) -DRAMULATOR -MM -MT $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC)) $(SRC) >> $(OBJDIR)/.depend ;)


ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif


lib_opt: $(OBJS)
	$(AR) rcs $@ $(OBJS)

lib_dbg: $(DBG_OBJS)
	$(AR) rcs $@ $(DBG_OBJS)

lib_gpf: $(GPF_OBJS)
	$(AR) rcs $@ $(GPF_OBJS)

$(OBJS): | $(OBJDIR)

$(DBG_OBJS): | $(OBJDIR_DBG)

$(GPF_OBJS): | $(OBJDIR_GPF)

$(OBJDIR): 
	@mkdir -p $@

$(OBJDIR_DBG): 
	@mkdir -p $@

$(OBJDIR_GPF): 
	@mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DRAMULATOR -c -o $@ $<

$(OBJDIR_DBG)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS_DBG) -DRAMULATOR -c -o $@ $<

$(OBJDIR_GPF)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS_GPF) -DRAMULATOR -c -o $@ $<
