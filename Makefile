PYODIDE_ROOT=$(abspath .)
include Makefile.envs

FILEPACKAGER=emsdk/emsdk/emscripten/tag-1.38.4/tools/file_packager.py

CPYTHONROOT=cpython
CPYTHONLIB=$(CPYTHONROOT)/installs/python-$(PYVERSION)/lib/python$(PYMINOR)

CC=emcc
CXX=em++
OPTFLAGS=-O3
CFLAGS=$(OPTFLAGS) -g -I$(PYTHONINCLUDE) -Wno-warn-absolute-paths
CXXFLAGS=$(CFLAGS) -std=c++14

# __ZNKSt3__220__vector_base_commonILb1EE20__throw_length_errorEv is in
# EXPORTED_FUNCTIONS to keep the C++ standard library in the core, even though
# there isn't any C++ there, for the sake of loading dynamic modules written in
# C++, such as those in matplotlib.
LDFLAGS=\
	-O3 \
	-s MODULARIZE=1 \
	$(CPYTHONROOT)/installs/python-$(PYVERSION)/lib/libpython$(PYMINOR).a \
  -s "BINARYEN_METHOD='native-wasm'" \
  -s TOTAL_MEMORY=536870912 \
	-s MAIN_MODULE=1 \
	-s EMULATED_FUNCTION_POINTERS=1 \
  -s EMULATE_FUNCTION_POINTER_CASTS=1 \
  -s EXPORTED_FUNCTIONS='["_main", "__ZNKSt3__220__vector_base_commonILb1EE20__throw_length_errorEv"]' \
  -s WASM=1 \
	-s SWAPPABLE_ASM_MODULE=1 \
	-s USE_FREETYPE=1 \
	-std=c++14 \
  -lstdc++ \
  --memory-init-file 0

SIX_ROOT=six/six-1.11.0/build/lib
SIX_LIBS=$(SIX_ROOT)/six.py

SITEPACKAGES=root/lib/python$(PYMINOR)/site-packages

all: build/pyodide.asm.js \
	build/pyodide.asm.data \
	build/pyodide.js \
	build/pyodide_dev.js \
	build/python.html \
	build/matplotlib.html \
	build/matplotlib-sideload.html \
	build/renderedhtml.css \
  build/test.data \
  build/packages.json


build/pyodide.asm.js: src/main.bc src/jsimport.bc src/jsproxy.bc src/js2python.bc \
											src/pyimport.bc src/pyproxy.bc src/python2js.bc \
											src/runpython.bc src/dummy_thread.bc src/hiwire.bc
	[ -d build ] || mkdir build
	$(CXX) -s EXPORT_NAME="'pyodide'" -o build/pyodide.asm.html $(filter %.bc,$^) \
	  $(LDFLAGS) -s FORCE_FILESYSTEM=1
	rm build/pyodide.asm.asm.js
	rm build/pyodide.asm.wasm.pre
	rm build/pyodide.asm.html


build/pyodide.asm.data: root/.built
	python2 $(FILEPACKAGER) build/pyodide.asm.data --preload root/lib@lib --js-output=build/pyodide.asm.data.js --use-preload-plugins
	uglifyjs build/pyodide.asm.data.js -o build/pyodide.asm.data.js


build/pyodide_dev.js: src/pyodide.js
	cp $< $@
	sed -i -e "s#{{DEPLOY}}##g" $@


build/pyodide.js: src/pyodide.js
	cp $< $@
	sed -i -e 's#{{DEPLOY}}#https://iodide.io/pyodide-demo/#g' $@


build/python.html: src/python.html
	cp $< $@


build/matplotlib.html: src/matplotlib.html
	cp $< $@


build/matplotlib-sideload.html: src/matplotlib-sideload.html
	cp $< $@


build/test.html: src/test.html
	cp $< $@


build/renderedhtml.css: src/renderedhtml.less
	lessc $< $@


test: all build/test.html
	py.test test -v


lint:
	flake8 src
	flake8 test
	flake8 tools/*
	clang-format -output-replacements-xml src/*.c src/*.h src/*.js | (! grep '<replacement ')


benchmark: all build/test.html
	python benchmark/benchmark.py $(HOSTPYTHON) build/benchmarks.json
	python benchmark/plot_benchmark.py build/benchmarks.json build/benchmarks.png


clean:
	rm -fr root
	rm build/*
	rm src/*.bc
	make -C packages clean
	make -C six clean
	echo "The Emsdk and CPython are not cleaned. cd into those directories to do so."


%.bc: %.c $(CPYTHONLIB)
	$(CC) -o $@ $< $(CFLAGS)


build/test.data: $(CPYTHONLIB)
	python2 $(FILEPACKAGER) build/test.data --preload $(CPYTHONLIB)/test@/lib/python3.6/test --js-output=build/test.js --export-name=pyodide --exclude \*.wasm.pre --exclude __pycache__
	uglifyjs build/test.js -o build/test.js


root/.built: \
		$(CPYTHONLIB) \
		$(SIX_LIBS) \
		src/sitecustomize.py \
		src/webbrowser.py \
		src/pyodide.py \
		remove_modules.txt
	rm -rf root
	mkdir -p root/lib
	cp -a $(CPYTHONLIB)/ root/lib
	cp $(SIX_LIBS) $(SITEPACKAGES)
	cp src/sitecustomize.py $(SITEPACKAGES)
	cp src/webbrowser.py root/lib/python$(PYMINOR)
	cp src/_testcapi.py	root/lib/python$(PYMINOR)
	cp src/pystone.py root/lib/python$(PYMINOR)
	cp src/pyodide.py root/lib/python$(PYMINOR)/site-packages
	( \
		cd root/lib/python$(PYMINOR); \
		rm -fr `cat ../../../remove_modules.txt`; \
		rm encodings/mac_*.py; \
		rm -fr test; \
		find . -name "*.wasm.pre" -type f -delete ; \
		find -type d -name __pycache__ -prune -exec rm -rf {} \; \
	)
	touch root/.built


$(CPYTHONLIB): emsdk/emsdk/emsdk
	make -C $(CPYTHONROOT)


$(SIX_LIBS): $(CPYTHONLIB)
	make -C six


build/packages.json: $(CPYTHONLIB)
	make -C packages


emsdk/emsdk/emsdk:
	make -C emsdk
