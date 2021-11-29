WASM_TARGET=js/pianolizer-wasm.js
TEST_BINARY=./test
CFLAGS=-O3 -std=c++14 -Wall -Werror

all: $(WASM_TARGET)

clean:
	@rm -f $(WASM_TARGET) $(TEST_BINARY)

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

test: cpp/test.cpp cpp/pianolizer.hpp
	@g++ $(CFLAGS) \
		-stdlib=libc++ \
		-o $(TEST_BINARY) \
		cpp/test.cpp
	@$(TEST_BINARY)