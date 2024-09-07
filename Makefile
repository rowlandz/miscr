.PHONY: help
help:
	@echo "Hello!"
	@echo ""
	@echo "Usage:"
	@echo "  make compiler     builds the compiler"
	@echo "  make playground   builds the playground"
	@echo "  make tests        builds the tests"

.PHONY: clean
clean:
	@./build.sh clean

compiler: src/main/main.cpp $(shell find src/main -name *.hpp)
	@./build.sh compiler

playground: $(shell find src/main -name *.hpp) src/play/*.cpp src/play/*.hpp
	@./build.sh playground

tests: $(shell find src/main -name *.hpp) src/test/*.cpp src/test/*.hpp
	@./build.sh tests
