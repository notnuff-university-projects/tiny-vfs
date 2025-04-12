#ifndef FUSE_BRIDGE_H
#define FUSE_BRIDGE_H

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "vfs_description.h"


// FuSeBridge
namespace fsb {

// I'm surprised this wrapper actually worked
struct FuseOperationsWrapper : public fuse_operations {
public:
  static fuse_operations& GetOperations();

protected:
  FuseOperationsWrapper();

protected:

  /**
   * Initialize filesystem
   *
   * The return value will passed in the `private_data` field of
   * `struct fuse_context` to all file operations, and as a
   * parameter to the destroy() method. It overrides the initial
   * value provided to fuse_main() / fuse_new().
   */
  static void* init(fuse_conn_info* conn, fuse_config* cfg);


  // stbuf - attributes about a file or vfs::Directory.
  static int getattr(const char* path, struct stat* stbuf, fuse_file_info* fi);

  static int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                     off_t offset, fuse_file_info* fi,
                     fuse_readdir_flags flags);

  static int open(const char* path, fuse_file_info* fi);

  static int read(const char* path, char* buf, size_t size, off_t offset,
                  fuse_file_info* fi);

  static int rename(const char* from, const char* to, unsigned int flags);

protected:
  template <typename T>
  static void FlushBuffer(T* buffer) {
    memset(buffer, 0, sizeof(T));
  }


  static bool CheckPermissionsForDescriptor(fuse_file_info* fi, vfs::TDescriptor desc);
  static bool CheckPermissionsForDescriptor(mode_t mode, vfs::TDescriptor desc);
  static bool CheckPermissions(mode_t mode, mode_t access, uid_t uid, gid_t gid);

  static vfs::TDescriptor FindDescriptor(const vfs::TDescriptor& root, const std::string& path);
  static vfs::TDescriptor GetRoot();

  struct PathSplit {
    std::string parent_path;
    std::string name;
  };

  static PathSplit SplitPath(const std::string &path);
};

struct ParsedOptions {
  bool show_help;
};

static void ShowHelp(const char *progname);
fuse_args PrepareFuseArgs(int argc, char *argv[]);


}


#endif //FUSE_BRIDGE_H