#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <memory>

#include "vfs_description.h"
#include "fuse_bridge.h"



int main(int argc, char *argv[]) {
    auto fs = vfs::PrepareFilesystemLayout();

    auto args = fsb::PrepareFuseArgs(argc, argv);
    auto ret = fuse_main(args.argc, args.argv, &fsb::FuseOperationsWrapper::GetOperations(), nullptr);
    fuse_opt_free_args(&args);

    return ret;
}
