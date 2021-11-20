WASM_TARGET=js/pianolizer-wasm.js
CFLAGS=-O3 -std=c++11 -Wall -Werror

all: $(WASM_TARGET)

clean:
	@rm -f $(WASM_TARGET)

$(WASM_TARGET): cpp/pianolizer.cpp cpp/pianolizer.hpp js/pianolizer-wrapper.js
	@emcc $(CFLAGS) \
		--bind \
		--post-js js/pianolizer-wrapper.js \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s BINARYEN_ASYNC_COMPILATION=0 \
		-s SINGLE_FILE=1 \
		-s WASM=1 \
		-o $(WASM_TARGET) \
		cpp/pianolizer.cpp
