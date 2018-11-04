/* mlib.cc (updated on 2018/04/18)
 * Copyright (C) 2017-2018 renny1398.
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
#include <locale>
#include <codecvt>
#include <stdexcept>
#include <regex>
#include "mlib.h"
#include "reader.h"

namespace {

std::map<std::string, mlib::KeyInfo> key_info_;

std::string relpath2abspath(const std::string& relpath) {
  std::string abspath;
  std::string curr_relpath;
#ifdef _WINDOWS
  static std::regex re("^(?:[A-Za-z]:)?[\\\\/]");
#else
  static std::regex re_root("^/.*$");
#endif
  std::smatch match;
  if (relpath.empty() || !std::regex_match(relpath, match, re_root)) {
    char* cwd = ::getcwd(nullptr, 0);
    abspath.assign(cwd);
    ::free(cwd);    
    abspath.append(1, mlib::kPathDelim);
    curr_relpath = relpath;
  } else {
    abspath = match.str();
    curr_relpath = relpath.substr(abspath.length(), std::string::npos);
  }
  while ( !curr_relpath.empty() ) {
    static std::regex re_child("^([\\\\/]*)(?:\\\\|/|$)");
    static std::regex re_abs_parent("^(.*[\\\\/])[^\\\\/]+[\\\\/]?$");
    if (!std::regex_match(curr_relpath, match, re_child)) {
      return std::string();
    }
    if (match[1].str() == "..") {
      curr_relpath.erase(0, match.str().length());
      if (!std::regex_match(abspath, match, re_abs_parent)) {
        return std::string();
      }
      abspath = match[1].str();
    } else if (match[1].str() == ".") {
      curr_relpath.erase(0, match.str().length());
    } else {
      curr_relpath.erase(0, match.str().length());
      abspath.append(match.str());
    }
  }
  std::replace(abspath.begin(), abspath.end(),
               mlib::kPathDelimNotUsed, mlib::kPathDelim);
  return abspath;
}

std::vector<mlib::MLibPtr> GetMLibEntryHistory(const std::string& mlib_name,
                                               const std::string& product,
                                               const std::string& mlib_internal_path) {
  static const char *ext[2] = { ".dat", ".lib" };
  std::vector<mlib::MLibPtr> mlib_entries;
  mlib_entries.reserve(22);
  const auto ext_pos = mlib_name.find_last_of('.', std::string::npos);
  const std::string mlib_name_without_ext =
      (ext_pos == std::string::npos) ? mlib_name : mlib_name.substr(0, ext_pos);
  const auto delim_pos = mlib_name_without_ext.find_last_of(mlib::kPathDelim, std::string::npos);
  const std::string name =
      (delim_pos == std::string::npos) ? mlib_name_without_ext : mlib_name_without_ext.substr(0, delim_pos);
  std::string fixed_mlib_name;
  for (int i = 0; i < 2; ++i) { // { 0 : ".dat", 1 : ".lib" }
    for (char j = '9'; '0' <= j; --j) {
      fixed_mlib_name.assign(mlib_name_without_ext);
      if (j != '0') fixed_mlib_name.append(1, j);
      fixed_mlib_name.append(ext[i]);
      mlib::MLibPtr p_mlib_base = mlib::MLib::Open(fixed_mlib_name, product);
      if (p_mlib_base == nullptr) continue;
      mlib::MLibPtr mlib = p_mlib_base->Child(name);
      if (mlib == nullptr) mlib = p_mlib_base;
      mlib = mlib->GetEntry(mlib_internal_path);
      if (mlib != nullptr) {
        mlib_entries.push_back(mlib);
      }
    }
  }
  return mlib_entries;
}

} // namespace

namespace mlib {

////////////////////////////////////////////////////////////////////////
// MLib Class Function Definitions
////////////////////////////////////////////////////////////////////////

MLib::MLib(const std::string &lib_name, Reader *reader)
  : parent_(), libname_(GenerateFullPath(lib_name)), reader_(reader), file_pos_(0) {
  verbose_= false;
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

std::map< std::string, std::weak_ptr<MLib> > MLib::opened_libs_;

MLibPtr MLib::Open(const std::string &filename, const std::string &product) {
  // check if the library with the given filename has already been open
  auto opened = opened_libs_.find(filename);
  if (opened != opened_libs_.end()) {
    if (opened->second.expired() == false) {
      return opened->second.lock();
    }
    opened_libs_.erase(opened);
  }

  Reader *reader = mlib::CreateReader(filename, product);
  if (reader == nullptr) return nullptr;
  char signature[4];
  reader->Read(0, 4, signature);

  MLibPtr ret;
  if (signature[0] == 'L' && signature[1] == 'I' &&
      signature[2] == 'B') {
    if (signature[3] == 'P') {
      const auto data_alignment = GetDataAlignment(product);
      ret = MLibPtr(new LIBP_t(filename, reader, data_alignment));
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

std::string MLib::GetLocation() const noexcept {
  std::string location = location_;
  std::replace(location.begin(), location.end(), '/', '|');
  return libname() + location + name();
}

const MLibPtr MLib::GetOrCreateChild(size_t i) noexcept {
  assert(i < children_.size());
  auto& p_child(children_.at(i));
  if (p_child.expired()) {
    MLibPtr p_new_child(CreateChild(i));
    p_child = p_new_child;
    p_new_child->self_ = p_child;
    return p_new_child;
  }
  const MLibPtr p_child_locked = p_child.lock();
  if (IsVerbose()) {
    std::cout << "[Info] MLib: '" << p_child_locked->GetName()
              << "' is already opened." << std::endl;
  }
  return p_child_locked;
}

MLibPtr MLib::Child(size_t i) noexcept {
  if (IsFile()) {
    return MLibPtr();
  }
  LoadChildInfo();
  if (children_.size() <= i) {
    return MLibPtr();
  }
  return GetOrCreateChild(i);
}

MLibPtr MLib::Child(const std::string &name) noexcept {
  if (IsFile()) {
    return MLibPtr();
  }
  LoadChildInfo();
  const auto it = child_name2index_.find(name);
  if (it != child_name2index_.cend()) {
    return GetOrCreateChild(it->second);
  } else {
    return MLibPtr();
  }
}

std::string MLib::GetChildName(size_t i) const noexcept {
  return std::string(GetChildNameAsCharArray(i));
}

std::vector<std::string> MLib::GetChildNameList() const noexcept {
  std::vector<std::string> ret;
  const auto count = GetChildNumber();
  ret.reserve(count);
  for (unsigned int i = 0; i < count; ++i) {
    ret.emplace_back(GetChildNameAsCharArray(i));
  }
  return ret;
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

MLibPtr MLib::GetEntry(const std::string &path, size_t index) noexcept {
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

MLibPtr MLib::GetEntry(const std::string &path) noexcept {
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

std::vector<MLibPtr> MLib::GetChildren() noexcept {
  if (IsFile()) return std::vector<MLibPtr>();
  LoadChildInfo();
  std::vector<MLibPtr> ret;
  const auto child_num = GetChildNumber();
  ret.reserve(child_num);
  for (unsigned int i = 0; i < child_num; ++i) {
    ret.push_back(GetOrCreateChild(i));
  }
  return ret;
}

bool MLib::HasOnlyDirectories() const noexcept {
  if (IsFile()) return false;
  const auto child_num = GetChildNumber();
  MLib* self = const_cast<MLib*>(this);
  for (unsigned int i = 0; i < child_num; ++i) {
    if (self->Child(i)->IsFile() == true) return false;
  }
  return true;
}

size_t MLib::Read(off_t offset, size_t size, void *dest) {
  const auto file_size = GetSize();
  if (static_cast<decltype(offset)>(file_size) <= offset) {
    return 0;
  }
  if (file_size < offset + size) {
    size = file_size - offset;
  }
  auto read_bytes = reader_->Read(GetFileBaseOffset() + offset, size, dest);
  if (size != 0 && read_bytes == 0) {
    std::cerr << "[WARN] MLib::Read(): failed to read data in '"
              << location() << kPathDelim << GetName()
              << "'. Check if the data and DATA_ALIGNMENT are correct."
              << std::endl;
  }
  return read_bytes;
}

size_t MLib::Read(size_t size, void *dest) {
  // if (IsDirectory()) {
  //   return 0;
  // }
  size_t ret = Read(file_pos_, size, dest);
  file_pos_ += ret;
  return ret;
}

off_t MLib::Tell() const noexcept {
  if ( !IsFile() ) return -1;
  return file_pos_;
}

off_t MLib::Seek(off_t new_pos, int whence) noexcept {
  // if (IsDirectory()) return -1;
  switch (whence) {
  case SEEK_SET:
    break;
  case SEEK_CUR:
    new_pos += file_pos_;
    break;
  case SEEK_END:
    new_pos += static_cast<off_t>(GetSize());
    break;
  default:
    return -1;
  }
  new_pos = std::max(static_cast<off_t>(0), new_pos);
  file_pos_ = std::min(new_pos, static_cast<off_t>(GetSize()));
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

LIB_t::LIB_t(const std::string &lib_name, Reader *reader)
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

bool LIB_t::IsFile() const noexcept {
  return IsOpen() &&
         (hdr_.signature[0] != 'L' || hdr_.signature[1] != 'I' ||
          hdr_.signature[2] != 'B' || hdr_.signature[3] != '\0');
}

unsigned int LIB_t::GetChildNumber() const noexcept {
  if ( !IsOpen() || IsFile() ) { return 0; }
  return hdr_.entry_count;
}

void LIB_t::DoLoadChildInfo() noexcept {
  const auto num = hdr_.entry_count;
  if (entries_.size() == num) return;
  entries_.assign(num, LIBENTRY());
  Seek(sizeof(LIBHDR), SEEK_SET);
  Read(sizeof(LIBENTRY) * num, &entries_[0]);
}

const char *LIB_t::GetChildNameAsCharArray(size_t i) const noexcept {
  assert(entries_.size() == hdr_.entry_count);
  return entries_[i].file_name;
}

MLib *LIB_t::CreateChild(size_t i) noexcept {
  assert(entries_.size() == hdr_.entry_count);
  return new LIB_t(this, entries_.at(i));
}

off_t LIB_t::GetFileBaseOffset() const noexcept {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBU_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBU_t::LIBU_t(const std::string &lib_name, Reader *reader)
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
  UTF16ToUTF8(entry_info.file_name, mb_name);
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

bool LIBU_t::IsFile() const noexcept {
  return IsOpen() &&
         (hdr_.signature[0] != 'L' || hdr_.signature[1] != 'I' ||
          hdr_.signature[2] != 'B' || hdr_.signature[3] != 'U');
}

unsigned int LIBU_t::GetChildNumber() const noexcept {
  if ( !IsOpen() || IsFile() ) { return 0; }
  return hdr_.entry_count;
}

void LIBU_t::DoLoadChildInfo() noexcept {
  const auto num = hdr_.entry_count;
  if (entries_.size() == num) return;
  entries_.assign(num, LIBUENTRY());
  Seek(sizeof(LIBUHDR), SEEK_SET);
  Read(sizeof(LIBUENTRY) * num, &entries_[0]);
}

const char *LIBU_t::GetChildNameAsCharArray(size_t i) const noexcept {
  assert(entries_.size() == hdr_.entry_count);
  return entries_[i].file_name;
}

MLib *LIBU_t::CreateChild(size_t i) noexcept {
  assert(entries_.size() == hdr_.entry_count);
  return new LIBU_t(this, entries_[i]);
}

off_t LIBU_t::GetFileBaseOffset() const noexcept {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBP_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBP_t::SharedObject::SharedObject(Reader *reader, unsigned int data_alignment) {

  static_assert(sizeof(LIBPHDR) == 16, "size of LIBPHDR must be 16");
  static_assert(sizeof(LIBPENTRY) == 32, "size of LIBPENTRY must be 32");

  size_t data_base_offset = reader->Read(0, sizeof(LIBPHDR), &hdr_);

  entries_.assign(hdr_.entry_count, LIBPENTRY());
  data_base_offset += reader->Read(data_base_offset, sizeof(LIBPENTRY) * hdr_.entry_count, &entries_[0]);

  file_offsets_.assign(hdr_.file_count, 0);
  data_base_offset += reader->Read(data_base_offset, sizeof(unsigned int) * hdr_.file_count, &file_offsets_[0]);

  data_base_offset_ = (data_base_offset + (data_alignment-1)) & ~(data_alignment-1);
  // std::cout << "[Debug] data_alignment = " << data_alignment
  //           << ", data base offset = " << std::hex << data_base_offset_ << std::dec << std::endl;
}

LIBP_t::LIBP_t(const std::string &lib_name, Reader *reader, unsigned int data_alignment)
  : MLib(lib_name, reader), shobj_(new SharedObject(reader, data_alignment)),
    entry_index_(0), name_(shobj_->entries_.at(0).file_name) {
  if (IsVerbose()) {
    std::cout << "[Info] LIBP: opened '" << libname() << "' (root)." << std::endl;
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
      std::cout << ", file size = " << GetSize()
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

bool LIBP_t::IsFile() const noexcept {
  if ( !IsOpen() ) return false;
  const unsigned int &flags = shobj_->entries_[entry_index_].flags;
  return (flags & (LIBPENTRY::kFlagFile | LIBPENTRY::kFlagFile2));
}

size_t LIBP_t::GetSize() const noexcept {
  if ( !IsFile() ) { return 0UL; }
  return static_cast<size_t>(shobj_->entries_.at(entry_index_).length);
}

unsigned int LIBP_t::GetChildNumber() const noexcept {
  if (IsFile()) { return 0; }
  return shobj_->entries_.at(entry_index_).length;
}

void LIBP_t::DoLoadChildInfo() noexcept {}

const char *LIBP_t::GetChildNameAsCharArray(size_t i) const noexcept {
  LIBPENTRY &entry = shobj_->entries_.at(entry_index_);
  const auto child_entry_index = entry.offset_index + i;
  return shobj_->entries_.at(child_entry_index).file_name;
}

MLib *LIBP_t::CreateChild(size_t i) noexcept {
  LIBPENTRY &entry = shobj_->entries_.at(entry_index_);
  const auto child_entry_index = entry.offset_index + i;
  if (shobj_->hdr_.entry_count <= child_entry_index) {
    std::cerr << "[ERROR] LIBP: entry index " << child_entry_index
              << " is greater than or equal to entry number.\n(entry number: "
              << shobj_->hdr_.entry_count << ", current entry name: "
              << entry.file_name << ')' << std::endl;
    std::exit(-1);
  }
  return new LIBP_t(shobj_, this, child_entry_index);
}

off_t LIBP_t::GetFileBaseOffset() const noexcept {
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
  : fp_(::fopen(name.c_str(), "rb")), name_() {
  if (fp_ != nullptr) {
    name_.assign(relpath2abspath(name));
  }
}

OSFile::OSFile(OSFile &&f) noexcept
  : fp_(f.fp_), name_(std::move(f.name_)) {
  f.fp_ = nullptr;
}

OSFile::~OSFile() {
  if (fp_) {
    ::fclose(fp_);
  }
}

bool OSFile::IsOpen() const noexcept {
  return (fp_ != nullptr);
}

bool OSFile::IsFile() const noexcept {
  return (fp_ != nullptr);
}

std::string OSFile::GetName() const noexcept {
  static std::regex re_name("[\\\\/]([^\\\\/]+)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_name)) {
    return match[1].str();
  } else {
    return std::string();
  }
}

std::string OSFile::GetLocation() const noexcept {
  static std::regex re_location("^(.*[\\\\/])(?:[^\\\\/]+)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_location)) {
    return match[1].str();
  } else {
    return std::string();
  }
}

std::string OSFile::GetFullPath() const noexcept {
  return name_;
}

size_t OSFile::GetSize() const noexcept {
  auto t = ::ftell(fp_);
  ::fseek(fp_, 0, SEEK_END);
  auto ret = ::ftell(fp_);
  ::fseek(fp_, t, SEEK_SET);
  return ret;
}

off_t OSFile::Seek(off_t offset, int whence) noexcept {
  off_t ret = ::ftell(fp_);
  if (fp_) {
    ::fseek(fp_, offset, whence);
    ret = ::ftell(fp_);
  }
  return ret;
}

size_t OSFile::Read(size_t size, void *dest) noexcept(false) {
  return ::fread(dest, 1, size, fp_);
}

////////////////////////////////////////////////////////////////////////
// Directory Class Definitions
////////////////////////////////////////////////////////////////////////

OSDirectory::OSDirectory(const std::string &path) {
  dirp_ = ::opendir(path.c_str());
  if (dirp_) {
    name_ = relpath2abspath(path);
    if (*name_.crbegin() == kPathDelim) {
      name_.pop_back();
    }
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

bool OSDirectory::IsOpen() const noexcept {
  return (dirp_ != nullptr);
}

bool OSDirectory::IsDirectory() const noexcept {
  return (dirp_ != nullptr);
}

std::string OSDirectory::GetName() const noexcept {
  static std::regex re_name("^.*[\\\\/]([^\\\\/]*)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_name)) {
    return match[1].str();
  } else {
    return std::string();
  }
}

std::string OSDirectory::GetLocation() const noexcept {
  static std::regex re_location("^(.*[\\\\/])(?:[^\\\\/]*)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_location)) {
    return match[1].str();
  } else {
    return std::string();
  }
}

std::string OSDirectory::GetFullPath() const noexcept {
  return name_;
}

OSFile* OSDirectory::OpenFile(const std::string &filename) const noexcept {
  std::string abs_filename = name_;
  if (filename[0] != kPathDelim) {
    abs_filename.reserve(abs_filename.size() + 1 + filename.size());
    abs_filename.append(1, kPathDelim);
  } else {
    abs_filename.reserve(abs_filename.size() + filename.size());
  }
  abs_filename.append(filename);
  OSFile *file = new OSFile(abs_filename);
  if (file->IsOpen() == false) {
    delete file;
    file = nullptr;
  }
  return file;
}

OSDirectory* OSDirectory::OpenDirectory(const std::string& dirname) const noexcept {
  if (dirp_ == nullptr) return nullptr;
  seekdir(dirp_, 0);
  OSDirectory* p_dir = nullptr;
  struct dirent* dp;
  while ((dp = readdir(dirp_)) != nullptr) {
    if (dirname != dp->d_name) continue;
#ifndef _WINDOWS
    if (dp->d_type != DT_DIR) continue;
#endif
    p_dir = new OSDirectory(GetFullPath() + kPathDelim + dp->d_name);
    if ( !p_dir->IsOpen() ) {
      delete p_dir;
      return nullptr;
    } else {
      return p_dir;
    }
  }
  return nullptr;
}

OSEntry* OSDirectory::OpenChild(const std::string& child_name) const noexcept {
  if (dirp_ == nullptr) return nullptr;
  seekdir(dirp_, 0);
  OSEntry* p_entry;
  struct dirent* dp;
  while ((dp = readdir(dirp_)) != nullptr) {
    if (child_name != dp->d_name) continue;
#ifdef _WINDOWS
    p_entry = new OSFile(child_name);
    if (p_entry->IsOpen() == false) {
      delete p_entry;
      p_entry = new OSDirectory(child_name);
    }
#else
    if (dp->d_type == DT_DIR) {
      p_entry = new OSDirectory(child_name);
    } else {
      p_entry = new OSFile(child_name);
    }
#endif
    if ( !p_entry->IsOpen() ) {
      delete p_entry;
      return nullptr;
    } else {
      return p_entry;
    }
  }
  return nullptr;
}

std::vector<OSEntry*> OSDirectory::GetChildren() const noexcept {
  if (dirp_ == nullptr) return std::vector<OSEntry*>();
  std::vector<OSEntry*> ret;
  seekdir(dirp_, 0);
  OSEntry* p;
  struct dirent* dp;
  while ((dp = readdir(dirp_)) != nullptr) {
    std::string child_name(name_);
    child_name.append(1, kPathDelim).append(dp->d_name);
#ifdef _WINDOWS
    p = new OSFile(child_name);
    if (p->IsOpen() == false) {
      delete p;
      p = new OSDirectory(child_name);
    }
#else
    if (dp->d_type == DT_DIR) {
      p = new OSDirectory(child_name);
    } else {
      p = new OSFile(child_name);
    }
#endif
    if (p->IsOpen() == false) {
      delete p;
      continue;
    }
    ret.push_back(p);
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////
// Versioned Entry Class Definitions
////////////////////////////////////////////////////////////////////////

VersionedEntry::VersionedEntry()
  : p_os_entry_(nullptr), p_curr_(nullptr), curr_ver_(0) {}

VersionedEntry::VersionedEntry(OSEntry* p_os_entry,
                               std::vector<MLibPtr>&& mlib_history) noexcept
  : p_os_entry_(p_os_entry), mlib_entries_(std::move(mlib_history)),
    p_curr_(nullptr), curr_ver_(0) {
  if (p_os_entry && p_os_entry->IsOpen()) {
    name_ = p_os_entry->GetFullPath();
  } else if ( !mlib_entries_.empty() ) {
    name_ = mlib_entries_[0]->GetFullPath();
  }
  SwitchVersion(-1);
}

VersionedEntry::VersionedEntry(const std::string& path,
                               const std::string& product) 
  : p_curr_(nullptr), curr_ver_(0) {
  // 1. Open an OS entry.
  OSEntry* p_os_entry = new OSFile(path);
  if (p_os_entry->IsOpen() == false) {
    delete p_os_entry;
    p_os_entry = new OSDirectory(path);
    if (p_os_entry->IsOpen() == false) {
      delete p_os_entry;
      p_os_entry = nullptr;
    }
  }
  p_os_entry_ = p_os_entry;
  // 2. Open mlib entries.
  std::string path_left = path;
  std::string path_right = "";
  while (path_left.empty() == false) {
#if 0
    std::printf("[Debug] try to open '%s?%s' (product: %s)\n",
                path_left.c_str(), path_right.c_str(), product.c_str());
#endif
    mlib_entries_ = GetMLibEntryHistory(path_left, product, path_right);
    if (mlib_entries_.empty() == false) {
      break;
    }
    // [left, right] : ["foo/bar", "baz"] => ["foo", "bar/baz"]
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
  if (p_os_entry_ != nullptr || mlib_entries_.empty() == false) {
    SwitchVersion(-1);
    name_ = relpath2abspath(path);
  }
}

VersionedEntry::VersionedEntry(VersionedEntry&& e) noexcept
  : p_os_entry_(e.p_os_entry_),
    mlib_entries_(std::move(e.mlib_entries_)),
    name_(std::move(e.name_)),
    p_curr_(e.p_curr_), curr_ver_(e.curr_ver_) {
  e.p_os_entry_ = nullptr;
}

VersionedEntry::~VersionedEntry() {
  if (p_os_entry_) {
    delete p_os_entry_;
  }
}

int VersionedEntry::GetLatestVersion() const noexcept {
  return (p_os_entry_ ? 1 : 0) + static_cast<int>(mlib_entries_.size());
}

int VersionedEntry::GetCurrentVersion() const noexcept {
  return curr_ver_;
}

void VersionedEntry::SwitchVersion(int new_version) noexcept {
  if (GetLatestVersion() == 0) return;
  if (new_version > 0) {
    int i = static_cast<int>(mlib_entries_.size()) - new_version;
    if (i >= 0) {
      p_curr_ = mlib_entries_[i].get();
      curr_ver_ = new_version;
    } else {
      if (p_os_entry_) {
        p_curr_ = p_os_entry_;
      } else {
        p_curr_ = mlib_entries_[0].get();
      }
      curr_ver_ = GetLatestVersion();
    }
  } else if (new_version < 0) {
    int i = (p_os_entry_ ? -2 : -1) - new_version;
    if (i >= static_cast<int>(mlib_entries_.size())) {
      i = static_cast<int>(mlib_entries_.size()) - 1;
    }
    if (i >= 0) {
      p_curr_ = mlib_entries_[i].get();
      curr_ver_ = static_cast<int>(mlib_entries_.size()) - i;
    } else {
      if (p_os_entry_) {
        p_curr_ = p_os_entry_;
      } else {
        p_curr_ = mlib_entries_[0].get();
      }
      curr_ver_ = GetLatestVersion();
    }
  }
}

bool VersionedEntry::IsOpen() const noexcept {
  return p_curr_ && p_curr_->IsOpen();
}

bool VersionedEntry::IsFile() const noexcept {
  return p_curr_ && p_curr_->IsFile();
}

bool VersionedEntry::IsDirectory() const noexcept {
  return p_curr_ && p_curr_->IsDirectory();
}

bool VersionedEntry::IsRaw() const noexcept {
  return p_curr_ && p_curr_->IsRaw();
}

std::string VersionedEntry::GetName() const noexcept {
  static std::regex re_name("^.*[\\\\/]([^\\\\/]*)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_name)) {
    // std::cout << name_ <<  " -> " << match.str() << std::endl;
    return match[1].str();
  } else {
    // std::cout << name_ << std::endl;
    return std::string();
  }
}

std::string VersionedEntry::GetLocation() const noexcept {
  static std::regex re_location("^(.*[\\\\/])(?:[^\\\\/]*)$");
  std::smatch match;
  if (std::regex_match(name_, match, re_location)) {
    return match[1].str();
  } else {
    return std::string();
  }
}

std::string VersionedEntry::GetFullPath() const noexcept {
  return name_;
}

size_t VersionedEntry::GetSize() const noexcept {
  if (p_curr_ == nullptr) return 0UL;
  return p_curr_->GetSize();
}

off_t VersionedEntry::Seek(off_t offset, int whence) noexcept {
  if (p_curr_ == nullptr) return -1;
  return p_curr_->Seek(offset, whence);
}

size_t VersionedEntry::Read(size_t size, void* dest) noexcept(false) {
  if (p_curr_ == nullptr) return 0UL;
  return p_curr_->Read(size, dest);
}

VersionedEntry* VersionedEntry::OpenChild(const std::string& child_name) const noexcept {
  OSEntry* p_os_child = nullptr;
  std::vector<MLibPtr> mlib_child_history;
  if (p_os_entry_ && p_os_entry_->IsDirectory()) {
    OSDirectory* p_osdir = dynamic_cast<OSDirectory*>(p_os_entry_);
    assert(p_osdir);
    p_os_child = p_osdir->OpenChild(child_name);
    if (p_os_child && !p_os_child->IsOpen()) {
      delete p_os_child;
      p_os_child = nullptr;
    }
  }
  mlib_child_history.reserve(mlib_entries_.size());
  for (auto& p_mlib : mlib_entries_) {
    assert(p_mlib != nullptr);
    if ( !p_mlib->IsDirectory() ) continue;
    auto p_mlib_child = p_mlib->GetEntry(child_name);
    if ( p_mlib_child == nullptr || !p_mlib_child->IsOpen() ) continue;
    mlib_child_history.push_back(std::move(p_mlib_child));
  }
  return new VersionedEntry(p_os_child, std::move(mlib_child_history));
}

std::vector<VersionedEntry*> VersionedEntry::GetChildren() const noexcept {
  std::map<std::string, VersionedEntry*> children_map;
  if (p_os_entry_ && p_os_entry_->IsDirectory()) {
    OSDirectory* p_osdir = dynamic_cast<OSDirectory*>(p_os_entry_);
    assert(p_osdir);
    std::vector<OSEntry*> os_children = p_osdir->GetChildren();
    for (auto& p_child : os_children) {
      const auto name = p_child->GetName();
      VersionedEntry* p_entry = new VersionedEntry;
      p_entry->p_os_entry_ = p_child;
      p_entry->name_ = name_ + kPathDelim + name;
      children_map.insert(std::make_pair(name, p_entry));
    }
  }
  for (auto& p_mlib : mlib_entries_) {
    if ( !p_mlib->IsDirectory() ) continue;
    std::vector<MLibPtr> mlib_children = p_mlib->GetChildren();
    for (auto& p_mlib_child : mlib_children) {
      const auto& name = p_mlib_child->name();
      auto it = children_map.find(name);
      if (it == children_map.end()) {
        VersionedEntry* p_entry = new VersionedEntry;
        p_entry->mlib_entries_.push_back(p_mlib_child);
        p_entry->name_ = name_ + kPathDelim + name;
        children_map.insert(std::make_pair(name, p_entry));
      } else {
        it->second->mlib_entries_.push_back(p_mlib_child);
      }
    }
  }
  std::vector<VersionedEntry*> children;
  children.reserve(children_map.size());
  for (auto it = children_map.begin(); it != children_map.end(); ) {
    it->second->SwitchVersion(curr_ver_);
    children.push_back(it->second);
    it = children_map.erase(it);
  }
  return children;
}

////////////////////////////////////////////////////////////////////////
// Convert UTF16 to UTF8
////////////////////////////////////////////////////////////////////////

static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
  conv_to_bytes;

size_t UTF16ToUTF8(const char *src, char *dst) {
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

std::string UTF16ToUTF8(const char16_t *src, size_t length) {
  return conv_to_bytes.to_bytes(src, src + length);
}

std::string UTF16ToUTF8(const std::u16string &src) {
  return conv_to_bytes.to_bytes(src);
}

////////////////////////////////////////////////////////////////////////
// LibReader Create Functions
////////////////////////////////////////////////////////////////////////

Reader *CreateReader(const std::string &filename, const std::string &product) {
  const KeyInfo *kinfo;
  if (FindKeyInfo(product, &kinfo) == false ||
      kinfo->cipher_type() == CipherType::kCipherNone) {
    return new PlainReader(filename);
  } else if (kinfo->cipher_type() == CipherType::kCipherCamellia128) {
    // return new EncryptedLibReader(fp, kinfo->key_string());
    return new CamelliaDecrypter(filename, kinfo->key_string());
  } else if (kinfo->cipher_type() == CipherType::kCipherEqualMoreThanSLT) {
    return new SecondCryptoDecrypter(filename, kinfo->key_string());
  } else {
    return nullptr;
  }
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
    buf = path.substr(pos, next_pos);
    if (buf.empty()) break;
    pos = next_pos;
    if (pos != std::string::npos) {
      ++pos;
    }
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

unsigned int GetDataAlignment(const std::string &product) {
  std::string product_toupper;
  product_toupper.resize(product.size());
  std::transform(product.cbegin(), product.cend(), product_toupper.begin(), ::toupper);
  const auto it = key_info_.find(product_toupper);
  if (it == std::end(key_info_)) return 0;
  const KeyInfo &keyinfo = it->second;
  return keyinfo.data_alignment();
}

} // namespace mlib
