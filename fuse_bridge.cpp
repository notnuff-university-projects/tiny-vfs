#include "fuse_bridge.h"

#include "common.h"

uid_t mount_uid;
gid_t mount_gid;

namespace fsb {
fuse_operations& FuseOperationsWrapper::GetOperations() {
  static fuse_operations op = FuseOperationsWrapper{};
  return op;
}

FuseOperationsWrapper::FuseOperationsWrapper(): fuse_operations() {
  fuse_operations::init = init;
  fuse_operations::getattr = getattr;
  fuse_operations::readdir = readdir;
  fuse_operations::open = open;
  fuse_operations::read = read;
}

void* FuseOperationsWrapper::init(fuse_conn_info* conn, fuse_config* cfg) {
  cfg->kernel_cache = 1;

  // here is the main trich
  static vfs::TDescriptor fs = vfs::PrepareFilesystemLayout();

  auto* ctx = fuse_get_context();
  mount_uid = ctx->uid;
  mount_gid = ctx->gid;

  return &fs;
}

int FuseOperationsWrapper::getattr(const char* path, struct stat* stbuf, fuse_file_info* fi) {
  // explicit way to suppress compiler warnings about not used value
  (void) fi;

  // flushing the buffer
  FlushBuffer(stbuf);

  // cool thing - it's just like in GLFW callbacks for resizing can access custom data
  auto root = GetRoot();
  auto desc = FindDescriptor(root, path);

  if (!desc) return -ENOENT;

  *stbuf = desc->Stats();
  return 0;
}

int FuseOperationsWrapper::readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info* fi, fuse_readdir_flags flags) {

  auto root = GetRoot();
  auto desc = FindDescriptor(root, path);
  auto dir = std::dynamic_pointer_cast<vfs::Directory>(desc);

  if (!dir) return -ENOTDIR;

  filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

  for ( const auto &entry : dir->GetInnerDescriptors() ) {
    filler(buf, entry->Name().c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
  }

  return 0;
}

int FuseOperationsWrapper::open(const char* path, fuse_file_info* fi) {
  auto root = GetRoot();
  auto desc = FindDescriptor(root, path);

  if (!desc) return -ENOENT;

  if ( !std::dynamic_pointer_cast<vfs::File>(desc) ) return -EISDIR;

  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;

  mode_t access = desc->Access();

  bool is_read = (fi->flags & O_ACCMODE) == O_RDONLY;
  bool is_write = (fi->flags & O_ACCMODE) == O_WRONLY || (fi->flags & O_ACCMODE) == O_RDWR;

  if (is_read && !CheckPermissions(R_OK, access, uid, gid))
    return -EACCES;

  if (is_write && !CheckPermissions(W_OK, access, uid, gid))
    return -EACCES;

  return 0;
}

int FuseOperationsWrapper::read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) {

  auto root = GetRoot();
  auto desc = FindDescriptor(root, path);

  if (!desc) return -ENOENT;

  auto file = std::dynamic_pointer_cast<vfs::File>(desc);
  if (!file) return -EISDIR;

  auto &data = file->GetData();
  size_t len = data.size();

  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;

  if (!CheckPermissions(R_OK, file->Access(), uid, gid))
    return -EACCES;

  if (offset < len) {
    if (offset + size > len) size = len - offset;
    memcpy(buf, data.data() + offset, size);
  } else {
    size = 0;
  }

  return size;
}

int FuseOperationsWrapper::rename(const char* from, const char* to, unsigned int flags) {
  return 0;
}

bool FuseOperationsWrapper::CheckPermissions(mode_t mode, mode_t access, uid_t uid, gid_t gid) {
  // owner
  if (uid == mount_uid) {
    return (access & (mode >> 6)) != 0;
  }
  // group
  if (gid == mount_gid) {
    return (access & (mode >> 3)) != 0;
  }
  // other
  return (access & mode) != 0;
}

vfs::TDescriptor FuseOperationsWrapper::FindDescriptor(const vfs::TDescriptor& root, const std::string& path)  {
  if (path == "/") return root;

  std::string clean_path = path;
  if (clean_path.front() == '/') clean_path.erase(0, 1);

  std::istringstream ss(clean_path);
  std::string token;
  auto current = std::dynamic_pointer_cast<vfs::Directory>(root);

  if (!current) return nullptr;

  while (std::getline(ss, token, '/')) {
    bool found = false;
    for (const auto &entry : current->GetInnerDescriptors() ) {
      if (entry->Name() == token) {
        if(auto next_dir = std::dynamic_pointer_cast<vfs::Directory>(entry)) {
          current = next_dir;
        } else if (ss.peek() == EOF) {
          return entry; // last component, and it's a file
        } else {
          return nullptr; // path continues, but this is not a vfs::Directory
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

vfs::TDescriptor FuseOperationsWrapper::GetRoot()  {
  return *static_cast<vfs::TDescriptor*>( fuse_get_context()->private_data );
}

void ShowHelp(const char* progname) {
  printf("usage: %s <mountpoint>\n\n", progname);
}


static const fuse_opt availableOptions[] = {
  {"-h", offsetof(ParsedOptions, show_help), true},
  {"--help", offsetof(ParsedOptions, show_help), true},
  FUSE_OPT_END
};

fuse_args PrepareFuseArgs(int argc, char* argv[]) {
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
    ShowHelp(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  // assert(fuse_opt_add_arg(&args, "-odefault_permissions") == 0);

  return args;
}
}