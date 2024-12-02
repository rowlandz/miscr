.PHONY: help
help:
	@echo "Usage:"
	@echo "  make miscrc          build the MiSCR compiler"
	@echo "  make miscrc-static   build statically-linked miscrc"
	@echo "  make playground      build the playground"
	@echo "  make tests           build unit tests"
	@echo "  make clean           removes previously built files"

.PHONY: clean
clean:
	@./build.sh clean

miscrc: src/main/main.cpp $(shell find src/main -name *.hpp)
	@./build.sh miscrc

miscrc-static: src/main/main.cpp $(shell find src/main -name *.hpp)
	@./build.sh miscrc-static

playground: $(shell find src/main -name *.hpp) src/play/*.cpp src/play/*.hpp
	@./build.sh playground

tests: $(shell find src/main -name *.hpp) src/test/*.cpp src/test/*.hpp
	@./build.sh tests
