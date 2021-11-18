all: cpp/pianolizer.cpp
	@emcc -O2 -Wall -Werror --bind \
		-s WASM=1 \
		-s BINARYEN_ASYNC_COMPILATION=0 \
		-s SINGLE_FILE=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		--post-js js/pianolizer-wrapper.js \
		cpp/pianolizer.cpp \
		-o js/pianolizer-wasm.js
