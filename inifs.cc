#include <iostream>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <cassert>
#include <iomanip>

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
		memset(stbuf, 0, sizeof(*stbuf));
		update_stat(stbuf);

		if ("/"sv == path) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		}

		std::string_view p = path+1;

		if (auto split = p.find('/'); split != std::string_view::npos) {
			auto const section    = p.substr(0, split);
			auto const target_key = p.substr(split + 1);
			if (auto const key = std::find(ini.section_keys(section), {}, target_key); key) {
				stbuf->st_mode = S_IFREG | 0644;
				stbuf->st_nlink = 1;
				stbuf->st_size = key.value().size();
				return 0;
			}
			return -ENOENT;
		}

		if (auto section = ini.sections(); section && (section = std::find(section, {}, p))) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		}
		if (auto key = ini.section_keys(); key && (key = std::find(key, {}, p))) {
			stbuf->st_mode = S_IFREG | 0644;
			stbuf->st_nlink = 1;
			stbuf->st_size = key.value().size();
			return 0;
		}

		return -ENOENT;
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
			for (auto key = ini.section_keys(); key; ++key) {
				filler(buf, key->c_str(), NULL, 0, {});
			}
			return 0;
		}

		bool section_found = false;
		for (auto key = ini.section_keys(path + 1); key; ++key) {
			section_found = true;
			filler(buf, key->c_str(), NULL, 0, {});
		}

		return section_found ? 0 : -ENOENT;
	}

	static int open(char const* path, fuse_file_info *fi)
	{
		auto const p = std::string_view(path + 1);
		auto const split = p.find('/');

		if (!std::find(ini.section_keys(split == std::string_view::npos ? "" : p.substr(0, split)), {}, p.substr(split + 1)))
			return -ENOENT;

		if ((fi->flags & O_ACCMODE) != O_RDONLY)
			return -EACCES;

		return 0;
	}

	static int read(char const* path, char *buf, size_t size, off_t offset, fuse_file_info *)
	{
		auto const p = std::string_view(path + 1);
		auto const split = p.find('/');

		if (auto key = std::find(ini.section_keys(split == std::string_view::npos ? "" : p.substr(0, split)), {}, p.substr(split + 1));
				key) {
			auto const data = key.value().data();
			auto const len = key.value().size();
			if ((unsigned)offset < len) {
				if (offset + size > len)
					size = len - offset;
				memcpy(buf, data + offset, size);
			} else {
				size = 0;
			}
		} else {
			return -ENOENT;
		}

		return size;
	}

	static int mkdir(char const* path, mode_t mode)
	{
#ifdef Debug_Mode
		std::cerr << "mkdir(" << std::quoted(path) << ", " << std::oct << mode << ")\n";
#else
		(void)mode;
#endif
		if (path == "/"sv)
			return -EEXIST;

		auto const p = std::string_view(path+1);
		if (p.find_first_of("/[]") != std::string_view::npos)
			return -EINVAL;

		auto &new_node = ini.nodes.emplace_back();
		new_node.kind = INI::Node::Kind::Section;
		new_node.value = p;
		return 0;
	}

	static int rmdir(char const* path)
	{
#ifdef Debug_Mode
		std::cerr << "rmdir(" << std::quoted(path) << ")\n";
#endif

		if (path == "/"sv)
			return -ENOTEMPTY;

		if (auto section = ini.sections(); section && (section = std::find(section, {}, path + 1))) {
			if (auto next = std::next(section); next ? next.node().kind == INI::Node::Kind::Section : 1) {
				ini.nodes.erase(ini.nodes.cbegin() + section.i);
				return 0;
			}
			return -ENOTEMPTY;
		}

		return -ENOTDIR;
	}

	int rename(char const* src, char const* dst, unsigned int flags)
	{
#ifdef Debug_Mode
		std::cerr << "rename(" << std::quoted(src) << ", " << std::quoted(dst) << ", " << flags << ")\n";
#endif

		if (dst == "/"sv || src == "/"sv)
			return -EPERM;

		auto const s = std::string_view(src+1);
		auto const d = std::string_view(dst+1);

		auto const s_split = s.find('/');
		auto const d_split = d.find('/');

		if (s_split == std::string_view::npos && d_split == std::string_view::npos) {
			if (d.find_first_of("[]") != std::string_view::npos) {
				return -EINVAL;
			}
			if (auto s_sec = ini.sections(); s_sec && (s_sec = std::find(s_sec, {}, s))) {
				if (auto d_sec = ini.sections(); d_sec && (d_sec = std::find(d_sec, {}, d))) {
					switch (flags) {
					case RENAME_EXCHANGE:
						std::swap(ini.nodes[s_sec.i].value, ini.nodes[d_sec.i].value);
						return 0;
					case RENAME_NOREPLACE:
						return -EEXIST;
					}

					if (auto d_next = std::next(d_sec); d_next ? d_next.node().kind == INI::Node::Kind::Section : 1) {
						ini.nodes.erase(ini.nodes.cbegin() + d_sec.i);
						return 0;
					}
					return -ENOTEMPTY;
				}

				ini.nodes[s_sec.i].value = d;
				return 0;
			}
			return -EEXIST;
		} else {
			return -ENOSYS;
		}
	}
}

int main(int argc, char **argv)
{
	fuse_operations oper = {};
	oper.getattr = inifs::getattr;
	oper.init    = inifs::init;
	oper.mkdir   = inifs::mkdir;
	oper.open    = inifs::open;
	oper.read    = inifs::read;
	oper.readdir = inifs::readdir;
	oper.rename  = inifs::rename;
	oper.rmdir   = inifs::rmdir;

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
