#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <cstddef>
#include <cassert>
#include <unistd.h>

class Descriptor {
public:
	Descriptor() = default;
	virtual ~Descriptor() = default;

	virtual mode_t Type() = 0;

};

class Directory : public Descriptor {
public:
	mode_t Type() override { return S_IFDIR; };

protected:

};

class File : public Descriptor {
public:
	mode_t Type() override { return S_IFREG; };

protected:


};


struct ParsedOptions {
	bool show_help;
};

static const fuse_opt availableOptions[] = {
	{"-h", offsetof(struct ParsedOptions, show_help), true},
	{"--help", offsetof(struct ParsedOptions, show_help), true},
	FUSE_OPT_END
};

static void show_help(const char *progname) {
	printf("usage: %s <mountpoint>\n\n", progname);
}


// static const struct fuse_operations hello_oper = {
// 	.init           = hello_init,
// 	.getattr	= hello_getattr,
// 	.readdir	= hello_readdir,
// 	.open		= hello_open,
// 	.read		= hello_read,
// };

// I'm surprised this wrapper actually worked
struct FuseOperationsWrapper : public fuse_operations {
	FuseOperationsWrapper() : fuse_operations() {

		// fuse_operations::init = init;
		fuse_operations::getattr = getattr;
		fuse_operations::readdir = readdir;
		fuse_operations::open = open;
		fuse_operations::read = read;
	}

protected:
	/**
	 * Initialize filesystem
	 *
	 * The return value will passed in the `private_data` field of
	 * `struct fuse_context` to all file operations, and as a
	 * parameter to the destroy() method. It overrides the initial
	 * value provided to fuse_main() / fuse_new().
	 */
	static void* init(fuse_conn_info *conn, fuse_config *cfg) {
		cfg->kernel_cache = 1;

		return nullptr;
	}


	// stbuf - attributes about a file or directory.
	static int getattr (const char *path, struct stat *stbuf, fuse_file_info *fi) {
		// explicit way to suppress compiler warnings about not used value
		(void) fi;

		// flushing the buffer
		flush_buffer(stbuf);

		if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;

			// link count is 2 because it counts for / and .
			stbuf->st_nlink = 2;
		} else if (strcmp(path, "/test") == 0) {
			stbuf->st_mode = S_IFREG | 0555;
			stbuf->st_nlink = 1;
			stbuf->st_size = 10;
		} else {
			return -ENOENT;
		}

		return 0;
	}

	static int readdir (const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, fuse_file_info *fi,
			 fuse_readdir_flags flags) {

		if (strcmp(path, "/") == 0) {
			filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
			filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
			filler(buf, "test", nullptr, 0, FUSE_FILL_DIR_PLUS);
		} else {
			return -ENOENT;
		}

		return 0;
	}

	static int open (const char *path, fuse_file_info *fi) {
		if (strcmp(path, "/test") != 0)
			return -ENOENT;

		if ((fi->flags & O_ACCMODE) != O_RDONLY)
			return -EACCES;

		return 0;
	}

	static int read (const char *path, char *buf, size_t size, off_t offset,
			  fuse_file_info *fi) {
		size_t len;
		if(strcmp(path, "/test") != 0)
			return -ENOENT;

		const auto content = "hi-hi";

		len = strlen(content);
		if (offset < len) {
			if (offset + size > len)
				size = len - offset;
			memcpy(buf, content + offset, size);
		} else
			size = 0;

		return size;
	}

protected:
	template <typename T>
	static void flush_buffer(T* buffer) {
		memset(buffer, 0, sizeof(T));
	}

};

static const FuseOperationsWrapper tinyVFSOperations{};

int main(int argc, char *argv[]) {
	fuse_args args = FUSE_ARGS_INIT(argc, argv);

	ParsedOptions opt{};
	// parse options - in our case it's -h or --help only
	if (fuse_opt_parse(&args, &opt, availableOptions, nullptr) == -1) return 1;

	// When --help is specified, first print our own file-system
	// specific help text, then signal fuse_main to show
	// additional help (by adding `--help` to the options again)
	// without usage: line (by setting argv[0] to the empty string)
	if (opt.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	// assert(fuse_opt_add_arg(&args, "-odefault_permissions") == 0);

	auto ret = fuse_main(args.argc, args.argv, &tinyVFSOperations, nullptr);
	fuse_opt_free_args(&args);
	return ret;
}