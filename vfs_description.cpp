#include "vfs_description.h"

#include "common.h"

namespace vfs {

struct stat Descriptor::Stats()  {
  struct stat result{};
  result.st_gid = mount_gid;
  result.st_uid = mount_uid;

  AddStats(result);

  return result;

}

Descriptor::Descriptor(mode_t access, const std::string& name)  {
  access_ = access;
  name_ = name;
}

void Descriptor::Access(mode_t acc)  { access_ = acc; }

mode_t Descriptor::Access()  { return access_; }

void Descriptor::Name(const std::string& name)  { name_ = name; }

const std::string& Descriptor::Name()  { return name_; }

void Directory::AddStats(struct stat& tarStats)  {
  tarStats.st_mode = S_IFDIR | access_;
}

const std::vector<TDescriptor>& Directory::GetInnerDescriptors() const  {
  return directory_layout_;
}

void File::AddStats(struct stat& tarStats) {
  tarStats.st_mode = S_IFREG | access_;
  tarStats.st_size = data_.size();
}

void File::Write(const std::vector<std::byte>& data) {
  data_ = data;
}

const std::vector<std::byte>& File::GetData() {
  return data_;
}

TDescriptor PrepareFilesystemLayout()  {
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


}