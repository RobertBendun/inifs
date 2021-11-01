#include <iostream>
#include <cstddef>
#include <cstring>

#define FUSE_USE_VERSION 31
#include <fuse.h>

static struct options {
	char const* ini_filename;
	int show_help;
} options;

#define OPTION(t, p) \
 { t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
	OPTION("--ini=%s", ini_filename),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

#define Content "hello, world!\n"

namespace inifs
{
	static void* init(fuse_conn_info *, fuse_config *cfg)
	{
		cfg->kernel_cache = 1;
		return NULL;
	}

	static int getattr(char const* path, struct stat *stbuf, fuse_file_info*)
	{
		int res = 0;

		memset(stbuf, 0, sizeof(*stbuf));
		if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (strcmp(path + 1, "xd") == 0) {
			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = strlen(Content);
		} else {
			res = -ENOENT;
		}

		return res;
	}

	static int readdir(
			char const* path, void *buf,
			fuse_fill_dir_t filler,
			off_t,
			fuse_file_info*,
			fuse_readdir_flags)
	{
		if (strcmp(path, "/") != 0)
			return -ENOENT;

		filler(buf, ".", NULL, 0, {});
		filler(buf, "..", NULL, 0, {});
		filler(buf, "xd", NULL, 0, {});
		return 0;
	}

	static int open(char const* path, fuse_file_info *fi)
	{
		if (strcmp(path+1, "xd") != 0)
			return -ENOENT;

		if ((fi->flags & O_ACCMODE) != O_RDONLY)
			return -EACCES;

		return 0;
	}

	static int read(char const* path, char *buf, size_t size, off_t offset, fuse_file_info *)
	{
		size_t len;

		if (strcmp(path+1, "xd") != 0)
			return -ENOENT;

		len = strlen(Content);
		if ((unsigned)offset < len) {
			if (offset + size > len)
				size = len - offset;
			memcpy(buf, Content + offset, size);
		} else {
			size = 0;
		}

		return size;
	}
}


int main(int argc, char **argv)
{
	fuse_operations oper = {};
	oper.getattr = inifs::getattr;
	oper.init = inifs::init;
	oper.readdir = inifs::readdir;
	oper.open = inifs::open;
	oper.read = inifs::read;

	fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (fuse_opt_parse(&args, &options, option_spec, nullptr) == -1)
		return 1;

	auto ret = fuse_main(args.argc, args.argv, &oper, nullptr);
	fuse_opt_free_args(&args);
	return ret;
}
