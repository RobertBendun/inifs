all: inifs

inifs: inifs.cc ini.cc
	g++ -std=c++20 -Wall -Wextra -o $@ $< `pkg-config fuse3 --cflags --libs` -Wno-missing-field-initializers

inifs-dbg: inifs.cc ini.cc
	g++ -DDebug_Mode -std=c++20 -Wall -Wextra -o $@ $< `pkg-config fuse3 --cflags --libs` -Wno-missing-field-initializers

.PHONY: mount
mount: inifs-dbg example.ini
	-umount test
	mkdir -p test
	./inifs-dbg --ini=example.ini -f test
