CMAKE=$(shell if which cmake3 ; then echo "cmake"; else echo "cmake"; fi)

all: opt

.PHONY: opt
opt: build/Makefile
	make -j --no-print-directory -C build 
	ln -sf build/checkpoint_loader checkpoint_loader

.PHONY: dbg
dbg: build_dbg/Makefile
	make -j --no-print-directory -C build_dbg
	ln -sf build_dbg/checkpoint_loader checkpoint_loader

build/Makefile:
	mkdir -p build
	cd build; $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release

build_dbg/Makefile:
	mkdir -p build_dbg
	cd build_dbg; $(CMAKE) .. -DCMAKE_BUILD_TYPE=Debug

clean:
	rm -rf build/ build_dbg/
	rm -f checkpoint_loader
