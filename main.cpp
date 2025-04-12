#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <memory>

#include "common.h"
#include "vfs_description.h"
#include "fuse_bridge.h"


uid_t mount_uid;
gid_t mount_gid;


int main(int argc, char *argv[]) {
    auto fs = vfs::PrepareFilesystemLayout();

    mount_uid = getuid();
    mount_gid = getgid();

    auto args = fsb::PrepareFuseArgs(argc, argv);
    auto ret = fuse_main(args.argc, args.argv, &fsb::FuseOperationsWrapper::GetOperations(), nullptr);
    fuse_opt_free_args(&args);

    return ret;
}
