#include "vfs_description.h"

#include <cstring>
#include <fstream>

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

void File::Write(const std::string& stringData) {
  data_ = std::vector<std::byte>(stringData.size());
  memcpy(data_.data(), stringData.data(), stringData.size());
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
                    test_txt->Write(
                        "blQGFah7eT\n"
                                  "scmFy44R2b\n"
                                  "7DUZt5j2br\n"
                                  "YlZPHJtXW5\n"
                                  "3uEkX8EQdi\n"
                                  "fpCU4zUOgm\n"
                                  "01BrgDblwA\n"
                                  "27PWcCZ1Wc\n"
                                  "73CSytk1C1\n"
                                  "lAqugFe4jN\n"
                                  "bN98h1QzyA\n"
                                  "qoJqJoGrra\n"
                                  "6H9foD1f52\n"
                                  "l7M4F38NU8\n"
                                );
                    auto cp = Descriptor::Create<File>(0544, "cp");

                    // open /bin/cp to write it to my cp file
                    std::ifstream cp_file("/bin/cp", std::ios::binary);
                    if (!cp_file) {
                      perror("Failed to open /bin/cp");
                      exit(EXIT_FAILURE);
                    }

                    std::vector<char> raw_data((std::istreambuf_iterator<char>(cp_file)),
                                               std::istreambuf_iterator<char>());

                    std::vector<std::byte> cp_data(raw_data.size());
                    memcpy(cp_data.data(), raw_data.data(), raw_data.size());

                    cp->Write(cp_data);

                    foo->AddDescriptors(cp, test_txt);
                }

                auto example = Descriptor::Create<File>(0544, "example");
                example->Write("SOS\n");
                auto readme_txt = Descriptor::Create<File>(0544, "readme.txt");
                readme_txt->Write("Yaroshenko Olekdandr Serhiyovych\n");
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