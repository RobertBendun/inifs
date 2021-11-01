all: ini-example inifs

inifs: inifs.cc ini.cc
	g++ -std=c++20 -Wall -Wextra -o $@ $< `pkg-config fuse3 --cflags --libs` -Wno-missing-field-initializers

ini-example: ini.cc example.ini
	g++ -std=c++20 -Wall -Wextra -o $@ $< -DIni_Example

hello-fuse: hello.c
	gcc -Wall $< `pkg-config fuse3 --cflags --libs` -o $@
