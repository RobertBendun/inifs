all: ini-example inifs

inifs: inifs.cc ini.cc
	g++ -std=c++20 -Wall -Wextra -o $@ $< `pkg-config fuse3 --cflags --libs` -Wno-missing-field-initializers
