.PHONY=cirno examples

cirno:
	gcc src/*/*.c src/*.c -o cirno

examples: cirno
	./cirno examples/bubble.9c
	./cirno examples/prime.9c
	./cirno examples/selection.9c
	./cirno examples/dot.9c
	./cirno examples/insertion.9c
