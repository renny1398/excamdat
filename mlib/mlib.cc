/* mlib.cc (updated on 2017/02/16)
 * Copyright (C) 2016 renny1398.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cassert>
#include <ctime>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
#include <stdexcept>
#include "mlib.h"
#include "reader.h"

namespace {

std::map<std::string, mlib::KeyInfo> key_info_;

size_t utf16_to_utf8(const char *src, char *dst) {
  const char *dst_start = dst;
  for (unsigned long s = 0; ; src += 2) {
    unsigned long n = src[1] * 0x100 + src[0];
    if (n == 0L) break;
    if (n <= 0x7f) {
      dst[0] = src[0];
      ++dst;
      continue;
    }
    if (n <= 0x7ff) {
      dst[0] = 0xc0 | (n >> 6);
      dst[1] = 0x80 | (n & 0x3f);
      dst += 2;
      continue;
    }
    if (0xd800 <= n && n <= 0xdbff) {
      s = n;
      continue;
    }
    if (0xdc00 <= n && n <= 0xdfff) {
      n = (s - 0xd800) * 0x400 + (n - 0xdc00) + 0x10000;
      dst[0] = 0xf0 | ((n >> 18) & 0x07);
      dst[1] = 0x80 | ((n >> 12) & 0x3f);
      dst[2] = 0x80 | ((n >> 6) & 0x3f);
      dst[3] = 0x80 | (n & 0x3f);
      dst += 4;
      continue;
    }
    dst[0] = 0xe0 | ((n >> 12) & 0x0f);
    dst[1] = 0x80 | ((n >> 6) & 0x3f);
    dst[2] = 0x80 | (n & 0x3f);
    dst += 3;
  }
  dst[0] = '\0';
  return dst - dst_start;
}

bool GetVersionedMLibEntry(const std::string &mlib_name, const std::string &product,
                           const std::string &mlib_internal_path,
                           std::vector<mlib::MLibPtr> *mlib_entries, bool is_file) {
  static const char *ext[2] = { ".dat", ".lib" };
  if (mlib_entries == nullptr) return false;
  // mlib_entries->clear();
  mlib_entries->reserve(mlib_entries->size() + 22);
  const auto ext_pos = mlib_name.find_last_of('.', std::string::npos);
  const std::string mlib_name_without_ext =
      (ext_pos == std::string::npos) ? mlib_name : mlib_name.substr(0, ext_pos);
  const auto delim_pos = mlib_name_without_ext.find_last_of(mlib::kPathDelim, std::string::npos);
  const std::string name =
      (delim_pos == std::string::npos) ? mlib_name_without_ext : mlib_name_without_ext.substr(0, delim_pos);
  std::string fixed_mlib_name;
  for (int i = 0; i < 2; ++i) {
    for (char j = '9'; '0' <= j; --j) {
      fixed_mlib_name.assign(mlib_name_without_ext);
      if (j != '0') fixed_mlib_name.append(1, j);
      fixed_mlib_name.append(ext[i]);
      mlib::MLibPtr mlib_base = mlib::MLib::Open(fixed_mlib_name, product);
      if (mlib_base == nullptr) continue;
      mlib::MLibPtr mlib = mlib_base->GetEntry(name + mlib::kPathDelim + mlib_internal_path);
      if (mlib != nullptr && mlib->IsFile() == is_file) {
        mlib_entries->push_back(mlib);
      } else {
        mlib = mlib_base->GetEntry(mlib_internal_path);
        if (mlib != nullptr && mlib->IsFile() == is_file) {
          mlib_entries->push_back(mlib);
        }
      }
    }
  }
  return (mlib_entries->empty() == false);
}

} // namespace

namespace mlib {

const std::string MLib::FormatOldStylePath(const std::string &mlib,
                                           const std::string &internal_path) {
  std::string ret = mlib;
  ret.reserve(mlib.size() + internal_path.size() + 2);
  ret.append(1, '|');
  for (auto &c : internal_path) {
    ret.append(1, (c == kPathDelim) ? '|' : c);
  }
  return ret;
}

const std::string MLib::FormatOldStylePath(const std::string &mlib,
                                           const std::string &location,
                                           const std::string &name) {
  std::string internal_path = location;
  internal_path.append(1, kPathDelim);
  internal_path.append(name);
  return FormatOldStylePath(mlib, internal_path);
}

////////////////////////////////////////////////////////////////////////
// MLib Class Function Definitions
////////////////////////////////////////////////////////////////////////

MLib::MLib(const std::string &lib_name, LibReader *reader)
  : parent_(), libname_(GenerateFullPath(lib_name)), reader_(reader), file_pos_(0) {
  verbose_= false;
  if (IsVerbose()) {
    std::cout << "[Info] MLib: opened and set fd " << reader->fd() << "." << std::endl;
  }
}

MLib::MLib(MLib *parent)
  : parent_(parent->self_.lock()), libname_(parent->libname_),
    reader_(parent->reader_), file_pos_(0) {
  verbose_ = parent->verbose_;
  location_.assign(parent->location_);
  if (location_.empty() == false) {
    location_.push_back(kPathDelim);
  }
  location_.append(parent->GetName());
}

MLib::~MLib() {
  children_.clear();
  if (reader_.unique()) {
    int fd = reader_->fd();
    reader_.reset();
    ::close(fd);
    if (IsVerbose()) {
      std::cout << "[Info] MLib: closed fd " << fd << "." << std::endl;
    }
  }
}

std::map< std::string, std::weak_ptr<MLib> > MLib::opened_libs_;

MLibPtr MLib::Open(const std::string &filename, const std::string &product) {
  auto opened = opened_libs_.find(filename);
  if (opened != opened_libs_.end()) {
    if (opened->second.expired() == false) {
      return opened->second.lock();
    }
    opened_libs_.erase(opened);
  }

  LibReader *reader = CreateReader(filename, product);
  if (reader == nullptr) return nullptr;
  char signature[4];
  reader->Read(0, 4, signature);
  MLibPtr ret = MLibPtr();
  if (signature[0] == 'L' && signature[1] == 'I' &&
      signature[2] == 'B') {
    if (signature[3] == 'P') {
      ret = MLibPtr(new LIBP_t(filename, reader, key_info_.at(product).data_alignment()));
    } else if (signature[3] == 'U') {
      ret = MLibPtr(new LIBU_t(filename, reader));
    } else {
      ret = MLibPtr(new LIB_t(filename, reader));
    }
  }
  if (ret.use_count()) {
    ret->self_ = ret;
    opened_libs_[filename] = ret;
  } else {
    delete reader;
  }
  return ret;
}

const std::string &MLib::GetLibraryName() const {
  return libname_;
}

const std::string &MLib::GetLocation() const {
  return location_;
}

bool MLib::IsDirectory() const {
  return (IsFile() == false);
}

const MLibPtr &MLib::Parent() {
  return parent_;
}

const MLibPtr MLib::CreateChild(size_t i) {
  auto &child(children_.at(i));
  if (child.expired()) {
    MLibPtr new_child(DoCreateChild(i));
    child = new_child;
    new_child->self_ = child;
    return new_child;
  }
  const MLibPtr p_child_locked = child.lock();
  if (IsVerbose()) {
    std::cout << "[Info] MLib: '" << p_child_locked->GetName()
              << "' is already opened." << std::endl;
  }
  return p_child_locked;
}

MLibPtr MLib::Child(size_t i) {
  if (IsFile()) {
    return MLibPtr();
  }
  LoadChildInfo();
  if (children_.size() <= i) {
    return MLibPtr();
  }
  return CreateChild(i);
}

MLibPtr MLib::Child(const std::string &name) {
  if (IsFile()) {
    return MLibPtr();
  }
  LoadChildInfo();
  const auto it = child_name2index_.find(name);
  if (it != child_name2index_.cend()) {
    return CreateChild(it->second);
  } else {
    return MLibPtr();
  }
}

const std::string MLib::GetChildName(size_t i) const {
  return std::string(GetChildNameAsCharArray(i));
}

void MLib::LoadChildInfo() {
  if (IsFile()) return;
  const auto child_num = GetChildNumber();
  if (children_.size() < child_num) {
   children_.assign(child_num, std::weak_ptr<MLib>());
  }
  DoLoadChildInfo();
  if (child_name2index_.size() == child_num) return;
  for (unsigned int i = 0; i < child_num; ++i) {
    child_name2index_[GetChildName(i)] = i;
  }
}

MLibPtr MLib::GetEntry(const std::string &path, size_t index) {
  const size_t delim_pos = path.find_first_of(kPathDelim, index);
  const std::string target_entry_name =
      (index == std::string::npos) ? "" :
      (path.substr(index, (delim_pos == std::string::npos) ? std::string::npos : delim_pos - index));
  const size_t sub_index =
      (delim_pos == std::string::npos) ? std::string::npos : delim_pos + 1;
  if (IsVerbose()) {
    std::cout << "[Info] MLib: try to open '" << path.substr(index) << "'." << std::endl;
  }

  if (target_entry_name == "") {
    assert(self_.expired() == false);
    return self_.lock();
  }
  if (IsFile()) {
    if (target_entry_name == ".") {
      const MLibPtr &parent = Parent();
      if (parent.use_count() == 0) return nullptr;
      return parent->GetEntry(path, sub_index);
    }
    if (target_entry_name == "..") {
      MLibPtr parent = Parent();
      if (parent.use_count() == 0) return parent;
      parent = parent->Parent();
      if (parent.use_count() == 0) return parent;
      return parent->GetEntry(path, sub_index);
    }
    return MLibPtr();
  }

  if (target_entry_name == ".") {
    return GetEntry(path, sub_index);
  }
  if (target_entry_name == "..") {
    return Parent()->GetEntry(path, sub_index);
  }

  MLibPtr child = Child(target_entry_name);
  if (child == nullptr) {
    return child;
  }
  if (sub_index == std::string::npos) {
    return child;
  }
  return child->GetEntry(path, sub_index);
}

MLibPtr MLib::GetEntry(const std::string &path) {
  std::string path_tmp(path);
  std::replace(path_tmp.begin(), path_tmp.end(), kPathDelimNotUsed, kPathDelim);
  if (path_tmp.back() == kPathDelim) {
    path_tmp.pop_back();
  }
  MLibPtr ret = GetEntry(path_tmp, 0);
  if (IsVerbose()) {
    if (ret == nullptr) {
      std::cout << "[Info] failed to open '" << path << "'." << std::endl;
    }
  }
  return ret;
}

bool MLib::GetAllChildren(std::vector<MLibPtr> *dest) {
  if (IsFile()) return false;
  if (dest == nullptr) return false;
  dest->clear();
  LoadChildInfo();
  const auto child_num = GetChildNumber();
  dest->reserve(child_num);
  for (unsigned int i = 0; i < child_num; ++i) {
    dest->push_back(CreateChild(i));
  }
  return true;
}

size_t MLib::Read(size_t size, void *dest) {
  // if (IsDirectory()) {
  //   return 0;
  // }
  size_t ret = reader_->Read(GetFileBaseOffset() + file_pos_, size, dest);
  file_pos_ += ret;
  return ret;
}

off_t MLib::Tell() const {
  if (IsDirectory()) return -1;
  return file_pos_;
}

off_t MLib::Seek(off_t new_pos, int whence) {
  // if (IsDirectory()) return -1;
  switch (whence) {
  case SEEK_SET:
    break;
  case SEEK_CUR:
    new_pos += file_pos_;
    break;
  case SEEK_END:
    new_pos += static_cast<off_t>(GetFileSize());
    break;
  default:
    return -1;
  }
  new_pos = std::max(static_cast<off_t>(0), new_pos);
  file_pos_ = std::min(new_pos, static_cast<off_t>(GetFileSize()));
  return file_pos_;
}

void MLib::List(const std::string &path) const {
  MLibPtr entry = const_cast<MLib*>(this)->GetEntry(path);
  if (entry == nullptr) {
    std::cerr << path << ": invalid path." << std::endl;
    return;
  }
  if (entry->IsFile()) {
    std::cout << "- " << entry->GetName() << std::endl;
  } else {
    for (unsigned int i = 0; i < GetChildNumber(); ++i) {
      MLibPtr child = entry->Child(i);
      if (child->IsFile()) {
        std::cout << "- ";
      } else {
        std::cout << "+ ";
      }
      std::cout << child->GetName() << std::endl;
    }
  }
}

////////////////////////////////////////////////////////////////////////
// LIB_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIB_t::LIB_t(const std::string &lib_name, LibReader *reader)
  : MLib(lib_name, reader), offset_(0), file_size_(reader->GetSize()) {
  reader->Read(0, sizeof(hdr_), &hdr_);
  if (IsVerbose()) {
    std::cout << "[Info] opened '" << GetName() << "'." << std::endl;
  }
}

LIB_t::LIB_t(LIB_t* parent, const LIBENTRY &entry_info)
  : MLib(parent), name_(entry_info.file_name),
    offset_(parent->offset_ + entry_info.offset),
    file_size_(entry_info.length) {
  Read(sizeof(hdr_), &hdr_);
  if (IsVerbose()) {
    std::cout << "[Info] opened '" << GetName() << "'." << std::endl;
  }
}

LIB_t::~LIB_t() {
  if (IsVerbose()) {
    std::cout << "[Info] closed '" << GetName() << "'." << std::endl;
  }
}

const std::string &LIB_t::GetName() const {
  return name_;
}

bool LIB_t::IsFile() const {
  return (hdr_.signature[0] != 'L' || hdr_.signature[1] != 'I' ||
          hdr_.signature[2] != 'B' || hdr_.signature[3] != '\0');
}

unsigned int LIB_t::GetFileSize() const {
  return file_size_;
}

unsigned int LIB_t::GetChildNumber() const {
  if (IsFile()) { return 0; }
  return hdr_.entry_count;
}

void LIB_t::DoLoadChildInfo() {
  const auto num = hdr_.entry_count;
  if (entries_.size() == num) return;
  entries_.assign(num, LIBENTRY());
  Seek(sizeof(LIBHDR), SEEK_SET);
  Read(sizeof(LIBENTRY) * num, &entries_[0]);
}

const char *LIB_t::GetChildNameAsCharArray(size_t i) const {
  assert(entries_.size() == hdr_.entry_count);
  return entries_[i].file_name;
}

MLib *LIB_t::DoCreateChild(size_t i) {
  assert(entries_.size() == hdr_.entry_count);
  return new LIB_t(this, entries_.at(i));
}

off_t LIB_t::GetFileBaseOffset() const {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBU_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBU_t::LIBU_t(const std::string &lib_name, LibReader *reader)
  : MLib(lib_name, reader), offset_(0), file_size_(reader->GetSize()) {
  reader->Read(0, sizeof(hdr_), &hdr_);
  if (IsVerbose()) {
    std::cout << "[Info] opened '" << GetName() << "'." << std::endl;
  }
}

LIBU_t::LIBU_t(LIBU_t *parent, const LIBUENTRY &entry_info)
  : MLib(parent), name_(),
    offset_(parent->offset_ + entry_info.offset),
    file_size_(entry_info.length) {
  char mb_name[32*5 + 1];
  utf16_to_utf8(entry_info.file_name, mb_name);
  name_.assign(mb_name);
  Read(sizeof(hdr_), &hdr_);
  if (IsVerbose()) {
    std::cout << "[Info] opened '" << GetName() << "'." << std::endl;
  }
}

LIBU_t::~LIBU_t() {
  if (IsVerbose()) {
    std::cout << "[Info] closed '" << GetName() << "'." << std::endl;
  }
}

const std::string &LIBU_t::GetName() const {
  return name_;
}

bool LIBU_t::IsFile() const {
  return (hdr_.signature[0] != 'L' || hdr_.signature[1] != 'I' ||
          hdr_.signature[2] != 'B' || hdr_.signature[3] != 'U');
}

unsigned int LIBU_t::GetFileSize() const {
  return file_size_;
}

unsigned int LIBU_t::GetChildNumber() const {
  if (IsFile()) { return 0; }
  return hdr_.entry_count;
}

void LIBU_t::DoLoadChildInfo() {
  const auto num = hdr_.entry_count;
  if (entries_.size() == num) return;
  entries_.assign(num, LIBUENTRY());
  Seek(sizeof(LIBUHDR), SEEK_SET);
  Read(sizeof(LIBUENTRY) * num, &entries_[0]);
}

const char *LIBU_t::GetChildNameAsCharArray(size_t i) const {
  assert(entries_.size() == hdr_.entry_count);
  return entries_[i].file_name;
}

MLib *LIBU_t::DoCreateChild(size_t i) {
  assert(entries_.size() == hdr_.entry_count);
  return new LIBU_t(this, entries_[i]);
}

off_t LIBU_t::GetFileBaseOffset() const {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBP_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBP_t::SharedObject::SharedObject(LibReader *reader, unsigned int data_alignment) {

  static_assert(sizeof(LIBPHDR) == 16, "size of LIBPHDR must be 16");
  static_assert(sizeof(LIBPENTRY) == 32, "size of LIBPENTRY must be 32");

  size_t data_base_offset = reader->Read(0, sizeof(LIBPHDR), &hdr_);

  entries_.assign(hdr_.entry_count, LIBPENTRY());
  data_base_offset += reader->Read(data_base_offset, sizeof(LIBPENTRY) * hdr_.entry_count, &entries_[0]);

  file_offsets_.assign(hdr_.file_count, 0);
  data_base_offset += reader->Read(data_base_offset, sizeof(unsigned int) * hdr_.file_count, &file_offsets_[0]);

  data_base_offset_ = (data_base_offset + (data_alignment-1)) & ~(data_alignment-1);
  std::cout << "[Debug] data_alignment = " << data_alignment
            << ", data base offset = " << std::hex << data_base_offset_ << std::dec << std::endl;
}

LIBP_t::LIBP_t(const std::string &lib_name, LibReader *reader, unsigned int data_alignment)
  : MLib(lib_name, reader), shobj_(new SharedObject(reader, data_alignment)),
    entry_index_(0), name_(shobj_->entries_.at(0).file_name) {
  if (IsVerbose()) {
    std::cout << "[Info] LIBP: opened '" << GetLibraryName() << "' (root)." << std::endl;
  }
}

LIBP_t::LIBP_t(const std::shared_ptr<SharedObject> &shobj, LIBP_t *parent, unsigned int entry_index)
  : MLib(parent), shobj_(shobj), entry_index_(entry_index), name_(shobj_->entries_.at(entry_index).file_name) {
  if (shobj_->entries_.at(entry_index).flags & LIBPENTRY::kFlagFile2) {
    std::cout << "[Debug] file flag of '" << name_ << "' is 0x20000." << std::endl;
  }
  if (IsVerbose()) {
    std::cout << "[Info] LIBP: opened '" << GetName()
              << "' (entry index = " << entry_index;
    if (IsFile()) {
      std::cout << ", file size = " << GetFileSize()
                << ", base offset = 0x" << std::hex << GetFileBaseOffset() << std::dec;
    }
    std::cout << ")." << std::endl;
  }
}

LIBP_t::~LIBP_t() {
  if (IsVerbose()) {
    std::cout << "[Info] LIBP: closed '" << GetName() << "'." << std::endl;
  }
}

const std::string &LIBP_t::GetName() const {
  return name_;
}

bool LIBP_t::IsFile() const {
  const unsigned int &flags = shobj_->entries_[entry_index_].flags;
  return (flags & (LIBPENTRY::kFlagFile | LIBPENTRY::kFlagFile2));
}

unsigned int LIBP_t::GetFileSize() const {
  if (IsDirectory()) { return 0; }
  return shobj_->entries_.at(entry_index_).length;
}

unsigned int LIBP_t::GetChildNumber() const {
  if (IsFile()) { return 0; }
  return shobj_->entries_.at(entry_index_).length;
}

void LIBP_t::DoLoadChildInfo() {}

const char *LIBP_t::GetChildNameAsCharArray(size_t i) const {
  LIBPENTRY &entry = shobj_->entries_.at(entry_index_);
  const auto child_entry_index = entry.offset_index + i;
  return shobj_->entries_.at(child_entry_index).file_name;
}

MLib *LIBP_t::DoCreateChild(size_t i) {
  LIBPENTRY &entry = shobj_->entries_.at(entry_index_);
  const auto child_entry_index = entry.offset_index + i;
  if (shobj_->hdr_.entry_count <= child_entry_index) {
    std::cerr << "[Error] LIBP: entry index " << child_entry_index
              << " is greater than or equal to entry number.\n(entry number: "
              << shobj_->hdr_.entry_count << ", current entry name: "
              << entry.file_name << ')' << std::endl;
    std::exit(-1);
  }
  return new LIBP_t(shobj_, this, child_entry_index);
}

off_t LIBP_t::GetFileBaseOffset() const {
  if (IsDirectory()) { return -1; }
  off_t base_offset = shobj_->file_offsets_.at(shobj_->entries_.at(entry_index_).offset_index);
  base_offset *= 1024;
  base_offset += shobj_->data_base_offset_;
  return base_offset;
}

////////////////////////////////////////////////////////////////////////
// File Class Definitions
////////////////////////////////////////////////////////////////////////

OSFile::OSFile(const std::string &name)
  : fp_(::fopen(name.c_str(), "rb")), name_(name) {}

OSFile::OSFile(OSFile &&f) noexcept
  : fp_(f.fp_), name_(std::move(f.name_)) {
  f.fp_ = nullptr;
}

OSFile::~OSFile() {
  if (fp_) {
    ::fclose(fp_);
  }
}

bool OSFile::IsOpened() const {
  return (fp_ != nullptr);
}

// WARNING: incompleted
const std::string OSFile::GetFullPath() const {
  return name_;
}

size_t OSFile::GetSize() const {
  auto t = ::ftell(fp_);
  ::fseek(fp_, 0, SEEK_END);
  auto ret = ::ftell(fp_);
  ::fseek(fp_, t, SEEK_SET);
  return ret;
}

off_t OSFile::Seek(off_t offset, int whence) {
  off_t ret = ::ftell(fp_);
  if (fp_) {
    ::fseek(fp_, offset, whence);
    ret = ::ftell(fp_);
  }
  return ret;
}

size_t OSFile::Read(size_t size, void *dest) {
  return ::fread(dest, 1, size, fp_);
}

MLibEmbbedFile::MLibEmbbedFile(const MLibPtr &mlib) : p_file_(mlib) {
  if (p_file_ != nullptr && p_file_->IsDirectory()) {
    p_file_.reset();
  }
}

MLibEmbbedFile::MLibEmbbedFile(const std::string &name, const std::string &product)
  : MLibEmbbedFile(MLib::Open(name, product)) {}

MLibEmbbedFile::MLibEmbbedFile(MLibEmbbedFile &&f) noexcept
  : p_file_(f.p_file_) {
  f.p_file_.reset();
}

MLibEmbbedFile::~MLibEmbbedFile() {}

bool MLibEmbbedFile::IsOpened() const {
  return (p_file_ != nullptr);
}

const std::string MLibEmbbedFile::GetFullPath() const {
  if (p_file_ == nullptr) {
    return std::string();
  }
  return MLib::FormatOldStylePath(p_file_->GetLibraryName(),
                                  p_file_->GetLocation(),
                                  p_file_->GetName());
}

size_t MLibEmbbedFile::GetSize() const {
  if (p_file_ == nullptr) return 0;
  return p_file_->GetFileSize();
}

off_t MLibEmbbedFile::Seek(off_t offset, int whence) {
  if (p_file_ == nullptr) return -1;
  return p_file_->Seek(offset, whence);
}

size_t MLibEmbbedFile::Read(size_t size, void *dest) {
  return p_file_->Read(size, dest);
}

VersionedFile::VersionedFile(const std::string &name, const std::string &product)
  : p_(nullptr), point_at_(-1) {
  OSFile *p_os_file = new OSFile(name);
  if (p_os_file->IsOpened()) {
    p_files_.push_back(p_os_file);
  } else {
    delete p_os_file;
    p_os_file = nullptr;
  }
  std::string path_left = name;
  std::string path_right = "";
  std::vector<MLibPtr> mlib_entries;
  while (path_left.empty() == false) {
    std::printf("[Debug] try to open '%s?%s' (product: %s)\n",
                path_left.c_str(), path_right.c_str(), product.c_str());
    if (GetVersionedMLibEntry(path_left, product, path_right, &mlib_entries, true)) {
      p_files_.reserve(mlib_entries.size());
      for (const auto &p_mlib : mlib_entries) {
        p_files_.push_back(new MLibEmbbedFile(p_mlib));
      }
      mlib_entries.clear();
      break;
    }
    const auto delim_pos = path_left.find_last_of(kPathDelim);
    if (delim_pos == std::string::npos) {
      break;
    }
    if (path_right.empty() == false) {
      path_right.insert(0, 1, kPathDelim);
    }
    path_right.insert(0, path_left.substr(delim_pos + 1));
    path_left.erase(delim_pos);
  }
  if (p_files_.empty() == false) {
    p_ = p_files_[0];
    point_at_ = static_cast<int>(p_files_.size());
  }
}

VersionedFile::VersionedFile(std::vector<File *> &&f) noexcept
  : p_files_(std::move(f)), p_(nullptr), point_at_(-1) {
  if (p_files_.empty() == false) {
    p_ = p_files_[0];
    point_at_ = static_cast<int>(p_files_.size());
  }
}

VersionedFile::~VersionedFile() {
  for (auto &p : p_files_) {
    if (p) delete p;
  }
}

bool VersionedFile::IsOpened() const {
  return (p_files_.empty() == false);
}

int VersionedFile::GetVersion() const {
  return static_cast<int>(p_files_.size());
}

int VersionedFile::GetPointingAt() const {
  return point_at_;
}

void VersionedFile::PointAt(int point_at) {
  const auto ver = GetVersion();
  if (ver <= point_at) {
    point_at = ver - 1;
  }
  p_ = p_files_.at(ver - point_at - 1);
  point_at_ = point_at;
}

const std::string VersionedFile::GetFullPath() const {
  if (p_ == nullptr) {
    return std::string();
  }
  return p_->GetFullPath();
}

size_t VersionedFile::GetSize() const {
  if (p_ == nullptr) {
    return 0;
  }
  return p_->GetSize();
}

off_t VersionedFile::Seek(off_t offset, int whence) {
  if (p_ == nullptr) {
    return 0;
  }
  return p_->Seek(offset, whence);
}

size_t VersionedFile::Read(size_t size, void *dest) {
  if (p_ == nullptr) {
    return 0;
  }
  return p_->Read(size, dest);
}

////////////////////////////////////////////////////////////////////////
// Directory Class Definitions
////////////////////////////////////////////////////////////////////////

OSDirectory::OSDirectory(const std::string &path) {
  dirp_ = ::opendir(path.c_str());
  if (dirp_) {
    const auto delim_pos = path.find_last_of(kPathDelim);
    name_ = (delim_pos == std::string::npos) ? path : path.substr(0, delim_pos);
  }
}

OSDirectory::OSDirectory(OSDirectory &&d) noexcept
  : dirp_(d.dirp_), name_(std::move(d.name_)) {
  d.dirp_ = nullptr;
}

OSDirectory::~OSDirectory() {
  if (dirp_) {
    ::closedir(dirp_);
  }
}

bool OSDirectory::IsOpened() const {
  return (dirp_ != nullptr);
}

// incomplete
const std::string OSDirectory::GetFullPath() const {
  return name_;
}

File* OSDirectory::OpenFile(const std::string &filename) const {
  std::string abs_filename = name_;
  if (filename[0] != kPathDelim) {
    abs_filename.append(kPathDelim, 1);
  }
  abs_filename.append(filename);
  OSFile *file = new OSFile(abs_filename);
  if (file->IsOpened() == false) {
    delete file;
    file = nullptr;
  }
  return file;
}

MLibDirectory::MLibDirectory(const MLibPtr &mlib)
  : p_dir_(mlib) {
  if (p_dir_->IsFile()) {
    p_dir_.reset();
  }
}

MLibDirectory::MLibDirectory(const std::string &path, const std::string &product)
  : MLibDirectory(MLib::Open(path, product)) {}

MLibDirectory::MLibDirectory(MLibDirectory &&d) noexcept
  : p_dir_(d.p_dir_) {
  d.p_dir_ = nullptr;
}

MLibDirectory::~MLibDirectory() {}

bool MLibDirectory::IsOpened() const {
  return (p_dir_ != nullptr);
}

const std::string MLibDirectory::GetFullPath() const {
  if (p_dir_ == nullptr) {
    return std::string();
  }
  return MLib::FormatOldStylePath(p_dir_->GetLibraryName(), p_dir_->GetLocation(), p_dir_->GetName());
}

File *MLibDirectory::OpenFile(const std::string &filename) const {
  if (p_dir_ == nullptr) {
    return nullptr;
  }
  MLibEmbbedFile *file = new MLibEmbbedFile(p_dir_->GetEntry(filename));
  if (file->IsOpened() == false) {
    delete file;
    file = nullptr;
  }
  return file;
}

VersionedDirectory::VersionedDirectory(const std::string &path, const std::string &product)
  : p_(nullptr), point_at_(-1) {
  OSDirectory *p_os_dir = new OSDirectory(path);
  if (p_os_dir->IsOpened()) {
    p_dirs_.push_back(p_os_dir);
  } else {
    delete p_os_dir;
    p_os_dir = nullptr;
  }
  std::string path_left = path;
  std::string path_right = "";
  std::vector<MLibPtr> mlib_entries;
  while (path_left.empty() == false) {
    std::printf("[Debug] try to open '%s?%s' (product: %s)\n",
                path_left.c_str(), path_right.c_str(), product.c_str());
    if (GetVersionedMLibEntry(path_left, product, path_right, &mlib_entries, false)) {
      p_dirs_.reserve(p_dirs_.size() + mlib_entries.size());
      for (const auto &p_mlib : mlib_entries) {
        p_dirs_.push_back(new MLibDirectory(p_mlib));
      }
      mlib_entries.clear();
      break;
    }
    const auto delim_pos = path_left.find_last_of(kPathDelim);
    if (delim_pos == std::string::npos) {
      break;
    }
    if (path_right.empty() == false) {
      path_right.insert(0, 1, kPathDelim);
    }
    path_right.insert(0, path_left.substr(delim_pos + 1));
    path_left.erase(delim_pos);
  }
  if (p_dirs_.empty() == false) {
    p_ = p_dirs_[0];
    point_at_ = static_cast<int>(p_dirs_.size());
  }
}

VersionedDirectory::~VersionedDirectory() {
  for (auto &p : p_dirs_) {
    if (p) delete p;
  }
}

bool VersionedDirectory::IsOpened() const {
  return (p_dirs_.empty() == false);
}

int VersionedDirectory::GetVersion() const {
  return static_cast<int>(p_dirs_.size());
}

int VersionedDirectory::GetPointingAt() const {
  return point_at_;
}

void VersionedDirectory::PointAt(int point_at) {
  const auto ver = GetVersion();
  if (ver <= point_at) {
    point_at = ver - 1;
  }
  p_ = p_dirs_.at(ver - point_at - 1);
  point_at_ = point_at;
}

const std::string VersionedDirectory::GetFullPath() const {
  if (p_ == nullptr) {
    return std::string();
  }
  return p_->GetFullPath();
}

VersionedFile *VersionedDirectory::OpenFile(const std::string &filename) const {
  std::vector<File *> files;
  files.reserve(p_dirs_.size());
  for (const auto &d : p_dirs_) {
    File *f = d->OpenFile(filename);
    if (f && f->IsOpened()) {
      files.push_back(f);
    } else {
      delete f;
    }
  }
  VersionedFile *ret = new VersionedFile(std::move(files));
  if (ret->IsOpened() == false) {
    delete ret;
    ret = nullptr;
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////
// LibReader Create Functions
////////////////////////////////////////////////////////////////////////

LibReader *CreateReader(const std::string &filename, const std::string &product) {
  int fd = ::open(filename.c_str(), O_RDONLY);
  if (fd == -1) return nullptr;
  const KeyInfo *kinfo;
  if (FindKeyInfo(product, &kinfo) == false ||
      kinfo->cipher_type() == CipherType::kCipherNone) {
    return new UnencryptedLibReader(fd);
  } else if (kinfo->cipher_type() == CipherType::kCipherCamellia128) {
    return new EncryptedLibReader(fd, kinfo->key_string());
  } else if (kinfo->cipher_type() == CipherType::kCipherEqualMoreThanSLT) {
    return new EncryptedLibReader2(fd, kinfo->key_string());
  }
  ::close(fd);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////
// Full-path Generating Functions
////////////////////////////////////////////////////////////////////////

std::string GenerateFullPath(const std::string &path) {
  char cwd[PATH_MAX];
  if (::getcwd(cwd, PATH_MAX) == nullptr) return std::string();
  std::string fullpath(cwd);
  std::string buf;
  size_t pos = 0;
  size_t next_pos = std::string::npos;
  do {
    next_pos = path.find_first_of(kPathDelim, pos);
    if (next_pos != std::string::npos) {
      next_pos -= 1;
    }
    buf = path.substr(pos, next_pos);
    if (buf.empty()) break;
    pos = next_pos;
    if (buf == ".") continue;
    if (buf == "..") {
      const size_t fullpath_last_delim_pos = fullpath.find_last_of(kPathDelim);
      if (fullpath_last_delim_pos == std::string::npos) return std::string();
      fullpath.erase(fullpath_last_delim_pos);
      continue;
    }
    fullpath.append(1, kPathDelim);
    fullpath.append(buf);
  } while (pos != std::string::npos);
  return fullpath;
}

////////////////////////////////////////////////////////////////////////
// Key Load Functions
////////////////////////////////////////////////////////////////////////

bool LoadKeyInfo(const std::string &csv) {
  std::ifstream ifs(csv);
  if (ifs.is_open() == false) return false;
  std::string l, product, rel_date, key, type, alignment;
  ifs >> l; // "PRODUCT_NAME,RELEASE_DATE,KEY_STRING,CIPHER_TYPE,DATA_ALIGNMENT
  KeyInfo info;
  while (ifs.eof() == false) {
    ifs >> l;
    std::istringstream iss(l);
    std::getline(iss, product, ',');
    std::getline(iss, rel_date, ',');
    std::getline(iss, key, ',');
    std::getline(iss, type, ',');
    std::getline(iss, alignment, ',');
    if (key.size() != 16) {
      if (key.size() != 32) continue;
      bool is_raw = false;
      for (const auto& c : key) {
        if ( ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') ||
             ('a' <= c && c <= 'f') ) continue;
        else {
          is_raw = true;
          break;
        }
      }
      if (is_raw) continue;
      for (int i = 0; i < 16; ++i) {
        std::string s(key.substr(2*i, 2));
        info.key_string_[i] = static_cast<unsigned char>(std::stoi(s, nullptr, 16));
      }
    } else {
      ::memcpy(info.key_string_, key.c_str(), 16);
    }
    info.cipher_type_ = std::stoi(type, nullptr, 10);
    info.data_alignment_ = std::stoi(alignment, nullptr, 10);
    key_info_.insert(std::make_pair(product, info));
  }
  return true;
}

bool FindKeyInfo(const std::string &product, const KeyInfo **dest) {
  if (dest == nullptr) return false;
  const auto it_key = key_info_.find(product);
  if (it_key != std::end(key_info_)) {
    *dest = &it_key->second;
    return true;
  }
  return false;
}

void PrintKeyInfo() {
  if (key_info_.empty()) return;
  std::cout << " PRODUCT   KEY_STRING                                           CIPHER_TYPE\n"
            << "--------- ---------------------------------------------------- -------------\n";
  std::cout.setf(std::ios::hex, std::ios::basefield);
  for (auto it = std::begin(key_info_); it != std::end(key_info_); ++it) {
    char fix_product_name[7+1];
    size_t fix_product_name_len = std::min(it->first.size(), static_cast<size_t>(7));
    ::memcpy(fix_product_name, it->first.c_str(), fix_product_name_len);
    fix_product_name[fix_product_name_len] = '\0';
    std::cout << ' ' << std::left << std::setfill(' ') << std::setw(7) << fix_product_name;
    std::cout << "   ";
    if (it->second.IsAsciiKey() == false) {
      std::cout << "{ ";
      const uint32_t *p = reinterpret_cast<const uint32_t *>(it->second.key_string());
      for (int i = 0; ; ) {
        std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << p[i];
        ++i;
        if (4 <= i) {
          std::cout << " }   ";
          break;
        }
        std::cout << ", ";
      }
    } else {
      char key_str[17];
      ::memcpy(key_str, it->second.key_string(), 16);
      key_str[16] = '\0';
      std::cout << '"' << key_str << "\"                                " << "   ";
    }
    switch (it->second.cipher_type()) {
    case CipherType::kCipherNone:
      std::cout << "Plain";
      break;
    case CipherType::kCipherCamellia128:
      std::cout << "Camellia128";
      break;
    case CipherType::kCipherEqualMoreThanSLT:
      std::cout << ">= SLT";
      break;
    default:
      std::cout << "Unknown";
    }
    std::cout << std::endl;
  }
  std::cout.setf(std::ios::dec, std::ios::basefield);
  std::cout << std::endl;
}

} // namespace mlib
