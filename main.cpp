#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <cstddef>
#include <cassert>
#include <memory>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>

class Descriptor;
using TDescriptor = std::shared_ptr<Descriptor>;

class Descriptor {
public:

    template<typename T>
    static std::shared_ptr<T> Create(mode_t access = 0755, const std::string &name = "") {
        auto desc = std::make_shared<T>();
        desc->Access(access);
        desc->Name(name);

        return desc;
    }

public:
    explicit Descriptor(mode_t access = 0755, const std::string &name = "") {
        access_ = access;
        name_ = name;
    }

    virtual ~Descriptor() = default;

public:
    virtual struct stat Stats() = 0;

    void Access(mode_t acc) { access_ = acc; }
    mode_t Access() { return access_; }

    void Name(const std::string& name) { name_ = name; }
    const std::string& Name() { return name_; }

protected:
    std::string name_;
    mode_t access_;
};

class Directory : public Descriptor {
public:
    struct stat Stats() override {
        struct stat result{};
        result.st_mode = S_IFDIR | access_;

        return result;
    }

    template<typename... TDescriptors>
    void AddDescriptors(const TDescriptor &fd, const TDescriptors &... fds) {
        directory_layout_.push_back(fd);
        AddDescriptors(fds...);
    }

    // empty function to end recursion from variadic variables
    void AddDescriptors() {}

    const std::vector<TDescriptor>& GetInnerDescriptors() const {
        return directory_layout_;
    }

protected:
    std::vector<TDescriptor> directory_layout_;
};

class File : public Descriptor {
public:
    struct stat Stats() override {
        struct stat result{};
        result.st_mode = S_IFREG | access_;
        result.st_size = data_.size();

        return result;
    }

    void Write(const std::vector<std::byte> &data) {
        data_ = data;
    }

protected:
    std::vector<std::byte> data_;
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


TDescriptor PrepareFilesystemLayout() {
    auto root = Descriptor::Create<Directory>(0755, "root");
    {
        auto bar = Descriptor::Create<Directory>(0755, "bar");
        {
            auto baz = Descriptor::Create<Directory>(0744, "baz");
            {
                auto foo = Descriptor::Create<Directory>(0711, "foo");
                {
                    auto test_txt = Descriptor::Create<File>(0444, "test.txt");
                    auto cp = Descriptor::Create<File>(0444, "cp");
                    foo->AddDescriptors(cp, test_txt);
                }

                auto example = Descriptor::Create<File>(0544, "example");
                auto readme_txt = Descriptor::Create<File>(0544, "readme.txt");
                auto bin = Descriptor::Create<Directory>(0177, "bin");
                baz->AddDescriptors(bin, readme_txt, example, foo);
            }
            bar->AddDescriptors(baz);
        }
        root->AddDescriptors(bar);
    }

    return root;
}

// I'm surprised this wrapper actually worked
struct FuseOperationsWrapper : public fuse_operations {
public:
    static fuse_operations& GetOperations() {
        static fuse_operations op = FuseOperationsWrapper{};
        return op;
    }

protected:
    FuseOperationsWrapper() : fuse_operations() {
        fuse_operations::init = init;
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
    static void *init(fuse_conn_info *conn, fuse_config *cfg) {
        cfg->kernel_cache = 1;

        // here is the main trich
        static TDescriptor fs = PrepareFilesystemLayout();
        return &fs;
    }


    // stbuf - attributes about a file or directory.
    static int getattr(const char *path, struct stat *stbuf, fuse_file_info *fi) {
        // explicit way to suppress compiler warnings about not used value
        (void) fi;

        // flushing the buffer
        flushBuffer(stbuf);

        // cool thing - it's just like in GLFW callbacks for resizing can access custom data
        auto root = *reinterpret_cast<TDescriptor*>( fuse_get_context()->private_data );
        auto desc = findDescriptor(root, path);

        if (!desc) return -ENOENT;

        *stbuf = desc->Stats();
        return 0;
    }

    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, fuse_file_info *fi,
                       fuse_readdir_flags flags) {

        auto root = *reinterpret_cast<TDescriptor*>( fuse_get_context()->private_data );
        auto desc = findDescriptor(root, path);
        auto dir = std::dynamic_pointer_cast<Directory>(desc);

        if (!dir) return -ENOENT;

        filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

        for ( const auto &entry : dir->GetInnerDescriptors() ) {
            filler(buf, entry->Name().c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        }

        return 0;
    }

    static int open(const char *path, fuse_file_info *fi) {
        if (strcmp(path, "/test") != 0)
            return -ENOENT;

        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;

        return 0;
    }

    static int read(const char *path, char *buf, size_t size, off_t offset,
                    fuse_file_info *fi) {
        size_t len;
        if (strcmp(path, "/test") != 0)
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
    template<typename T>
    static void flushBuffer(T *buffer) {
        memset(buffer, 0, sizeof(T));
    }


    static TDescriptor findDescriptor(const TDescriptor& root, const std::string &path) {
        if (path == "/") return root;

        std::string clean_path = path;
        if (clean_path.front() == '/') clean_path.erase(0, 1);

        std::istringstream ss(clean_path);
        std::string token;
        auto current = std::dynamic_pointer_cast<Directory>(root);

        if (!current) return nullptr;

        while (std::getline(ss, token, '/')) {
            bool found = false;
            for (const auto &entry : current->GetInnerDescriptors() ) {
                if (entry->Name() == token) {
                    if(auto next_dir = std::dynamic_pointer_cast<Directory>(entry)) {
                        current = next_dir;
                    } else if (ss.peek() == EOF) {
                        return entry; // last component, and it's a file
                    } else {
                        return nullptr; // path continues, but this is not a directory
                    }
                    found = true;
                    break;
                }
            }

            if (!found)
                return nullptr;
        }

        return current;
    }

    /*
    TDescriptor findDescriptor(const TDescriptor& currentDescriptor, const std::string& path) {
        if (currentDescriptor == nullptr) perror("passed descriptor is nullptr!"); // this should never happen

        auto tokenizedPath = tokenizePath(path);


        findDescriptor(currentDescriptor, tokenizedPath);
    }

    TDescriptor findDescriptor(const TDescriptor& currentDescriptor, const std::vector<std::string>& tokenizedCurrentPath) {
        if (currentDescriptor == nullptr) perror("passed descriptor is nullptr!"); // this should never happen

        if ( tokenizedCurrentPath.size() == 1 && currentDescriptor->Name() == tokenizedCurrentPath.front() )
    }

    std::vector<std::string> tokenizePath(const std::string& path) {
        std::vector<std::string> result;
        if (path.empty()) return result;

        size_t start = 0;
        size_t end = path.find('/');

        if (end == 0) {
            start = 1;
            end = path.find('/', start);
        }

        while (end != std::string::npos) {
            result.push_back(path.substr(start, end - start));
            start = end + 1;
            end = path.find('/', start);
        }

        if (start <= path.length()) {
            result.push_back(path.substr(start));
        }

        return result;
    }
    */

};

fuse_args PrepareFuseArgs(int argc, char *argv[]) {
    fuse_args args = FUSE_ARGS_INIT(argc, argv);

    ParsedOptions opt{};
    // parse options - in our case it's -h or --help only
    if (fuse_opt_parse(&args, &opt, availableOptions, nullptr) == -1) {
        perror("failed to parse fuse arguments");
        exit(EXIT_FAILURE);
    }

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

    return args;
}


int main(int argc, char *argv[]) {
    auto fs = PrepareFilesystemLayout();

    auto args = PrepareFuseArgs(argc, argv);
    auto ret = fuse_main(args.argc, args.argv, &FuseOperationsWrapper::GetOperations(), nullptr);
    fuse_opt_free_args(&args);

    return ret;
}
