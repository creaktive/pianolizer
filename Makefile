CPP=g++
EMCC=emcc
RM=rm
STRIP=strip

WASM_TARGET=js/pianolizer-wasm.js
TEST_BINARY=test
NATIVE_BINARY=pianolizer

# https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings
CFLAGS=-ffast-math -flto -std=c++14 -pedantic \
	-Werror -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 \
	-Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast \
	-Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo \
	-Wstrict-overflow=5 -Wswitch-default -Wno-unused \
	#-fsanitize=address
	#-Wlogical-op -Wnoexcept -Wstrict-null-sentinel -Wundef

all: $(NATIVE_BINARY) $(WASM_TARGET)

clean:
	$(RM) -f $(WASM_TARGET) $(TEST_BINARY) $(NATIVE_BINARY)

emscripten: $(WASM_TARGET)
$(WASM_TARGET): cpp/pianolizer.cpp cpp/pianolizer.hpp js/pianolizer-wrapper.js
	$(EMCC) $(CFLAGS) $(DEFS) \
		-O3 \
		--bind \
		--post-js js/pianolizer-wrapper.js \
		-s BINARYEN_ASYNC_COMPILATION=0 \
		-s EXPORTED_FUNCTIONS="['_malloc']" \
		-s SINGLE_FILE=1 \
		-s WASM=1 \
		-o $(WASM_TARGET) \
		cpp/pianolizer.cpp

$(TEST_BINARY): cpp/test.cpp cpp/pianolizer.hpp
	$(CPP) $(CFLAGS) $(DEFS) \
		-Ofast \
		-o $(TEST_BINARY) \
		cpp/test.cpp \
		-lgtest -lgtest_main
	$(STRIP) $(TEST_BINARY)
	./$(TEST_BINARY)

$(NATIVE_BINARY): cpp/main.cpp cpp/pianolizer.hpp
	$(CPP) $(CFLAGS) $(DEFS) \
		-Ofast \
		-o $(NATIVE_BINARY) \
		cpp/main.cpp
	$(STRIP) $(NATIVE_BINARY)
