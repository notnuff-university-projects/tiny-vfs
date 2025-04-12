#ifndef VFS_DESCRIPTION_H
#define VFS_DESCRIPTION_H


#include <memory>
#include <fcntl.h>

#include <algorithm>
#include <cstddef>
#include <vector>
#include <string>

// #include "fuse_bridge.h"

namespace vfs {
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class Descriptor {
public:
  template <typename T>
  static std::shared_ptr<T> Create(mode_t access = 0755, const std::string& name = "") {
    auto desc = std::make_shared<T>();
    desc->Access(access);
    desc->Name(name);

    return desc;
  }

public:
  explicit Descriptor(mode_t access = 0755, const std::string& name = "");

  virtual ~Descriptor() = default;

public:
  struct stat Stats();

  virtual void AddStats(struct stat& tarStats) {};

  void Access(mode_t acc);

  mode_t Access();

  void Name(const std::string& name);

  const std::string& Name();

protected:
  std::string name_;
  mode_t access_;
};


using TDescriptor = std::shared_ptr<Descriptor>;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class Directory : public Descriptor {
public:
  void AddStats(struct stat& tarStats) override;

  template <typename... TDescriptors>
  void AddDescriptors(const TDescriptor& fd, const TDescriptors&... fds) {
    directory_layout_.push_back(fd);
    AddDescriptors(fds...);
  }

  // empty function to end recursion from variadic variables
  void AddDescriptors() {}

  // return value - is success
  bool RemoveDescriptor(const TDescriptor& fd) {
    auto it = std::ranges::find(directory_layout_, fd);
    if (it == directory_layout_.end())
      return false;

    directory_layout_.erase(it);
    return true;
  }

  const std::vector<TDescriptor>& GetInnerDescriptors() const;

protected:
  std::vector<TDescriptor> directory_layout_;
};


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


class File : public Descriptor {
public:
  void AddStats(struct stat& tarStats) override;

  void Write(const std::vector<std::byte>& data);

  const std::vector<std::byte>& GetData();

protected:
  std::vector<std::byte> data_;
};

TDescriptor PrepareFilesystemLayout();

}


#endif //VFS_DESCRIPTION_H