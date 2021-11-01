#include <iostream>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <cassert>

#define FUSE_USE_VERSION 31
#include <fuse.h>

#include "ini.cc"

using namespace std::string_view_literals;

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

INI ini;
struct stat ini_stat;

inline void update_stat(struct stat *dst)
{
	dst->st_ctime = ini_stat.st_ctime;
	dst->st_atime = ini_stat.st_atime;
	dst->st_mtime = ini_stat.st_mtime;

	dst->st_uid = ini_stat.st_uid;
	dst->st_gid = ini_stat.st_gid;
}

namespace inifs
{
	static void* init(fuse_conn_info *, fuse_config *cfg)
	{
		cfg->kernel_cache = 1;
		return NULL;
	}

	static int getattr(char const* path, struct stat *stbuf, fuse_file_info*)
	{
		int res = -ENOENT;

		memset(stbuf, 0, sizeof(*stbuf));
		if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			res = 0;
		} else {
			std::string_view p = path+1;
			auto split = p.find('/');
			if (split == std::string_view::npos) {
				if (auto section_it = std::find(ini.sections(), {}, path + 1); section_it) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_size = 0; // TODO add size
					update_stat(stbuf);
					return 0;
				}
			} else {
				auto target_key = p.substr(split + 1);
				ini.for_section(p.substr(0, split), [&](std::string const& key, std::string const& value) {
					if (key == target_key) {
						stbuf->st_mode = S_IFREG | 0644;
						stbuf->st_nlink = 1;
						stbuf->st_size = value.size();
						update_stat(stbuf);
						res = 0;
					}
				});
			}
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
		filler(buf, ".", NULL, 0, {});
		filler(buf, "..", NULL, 0, {});
		if (path == "/"sv) {
			for (auto section = ini.sections(); section; ++section) {
				filler(buf, section->c_str(), NULL, 0, {});
			}
			return 0;
		}

		bool section_found = ini.for_section(path + 1, [&](std::string const& key, std::string const&) {
			filler(buf, key.c_str(), NULL, 0, {});
		});

		return section_found ? 0 : -ENOENT;
	}

	static int open(char const* path, fuse_file_info *fi)
	{
		auto p = std::string_view(path + 1);
		auto split = p.find('/');

		if (split == std::string::npos) {
			// Sectionless entries
			assert(false && "unimplemented");
		} else {
			auto const target_key = p.substr(split + 1);
			bool key_found = false;
			ini.for_section(p.substr(0, split), [&](std::string const& key, std::string const&) {
				if (target_key == key) {
					key_found = true;
				}
			});

			if (!key_found)
				return -ENOENT;

			if ((fi->flags & O_ACCMODE) != O_RDONLY)
				return -EACCES;
		}

		return 0;
	}

	static int read(char const* path, char *buf, size_t size, off_t offset, fuse_file_info *)
	{
		auto p = std::string_view(path + 1);
		auto split = p.find('/');

		if (split == std::string::npos) {
			// Sectionless entries
			assert(false && "unimplemented");
		} else {
			auto const target_key = p.substr(split + 1);
			bool key_found = false;
			ini.for_section(p.substr(0, split), [&](std::string const& key, std::string const& value) {
				if (target_key == key) {
					key_found = true;
					if ((unsigned)offset < value.size()) {
						if (offset + size > value.size())
							size = value.size() - offset;
						memcpy(buf, value.data() + offset, size);
					} else {
						size = 0;
					}
				}
			});

			if (!key_found)
				return -ENOENT;
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

	if (!options.ini_filename) {
		std::cerr << "inifs: no INI file was provided\n";
		return 2;
	}

	if (auto maybe_ini = INI::from_file(options.ini_filename); maybe_ini) {
		ini = *std::move(maybe_ini);
	} else {
		std::cerr << "inifs: invalid INI file\n";
		return 3;
	}

	lstat(options.ini_filename, &ini_stat);

	auto ret = fuse_main(args.argc, args.argv, &oper, nullptr);
	fuse_opt_free_args(&args);
	return ret;
}
