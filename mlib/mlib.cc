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
#include <ctime>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
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

} // namespace

namespace mlib {

////////////////////////////////////////////////////////////////////////
// MLib Class Function Definitions
////////////////////////////////////////////////////////////////////////

MLib::MLib(LibReader *reader) : parent_(nullptr), reader_(reader), file_pos_(0) {
  verbose_= true;
}

MLib::MLib(MLib *parent) : parent_(parent), reader_(parent->reader_), file_pos_(0) {
  verbose_ = parent->verbose_;
  location_.assign(parent->location_);
  if (location_.empty() == false) {
#ifdef _WINDOWS
    location_.push_back('\\');
#else
    location_.push_back('/');
#endif
  }
  location_.append(parent->GetName());
}

MLib::~MLib() {
  while (children_.empty() == false) {
    MLib *child = children_.back();
    children_.pop_back();
    delete child;
  }
  if (reader_.unique()) {
    int fd = reader_->fd();
    reader_.reset();
    ::close(fd);
  }
}

MLib *MLib::Open(const std::string &filename, const std::string &product) {
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) return nullptr;
  LibReader *reader;
  const KeyInfo *kinfo;
  if (FindKeyInfo(product, &kinfo) == false || kinfo->cipher_type() == 0) {
    reader = new UnencryptedLibReader(fd);
  } else if (kinfo->cipher_type() == 1) {
    reader = new EncryptedLibReader(fd, kinfo->key_string());
  } else if (kinfo->cipher_type() == 2) {
    reader = new EncryptedLibReader2(fd, kinfo->key_string());
  } else {
    return nullptr;
  }
  char signature[4];
  reader->Read(0, 4, signature);
  if (signature[0] == 'L' && signature[1] == 'I' &&
      signature[2] == 'B') {
    if (signature[3] == 'P') {
      return new LIBP_t(reader);
    }
    if (signature[3] == 'U') {
      return new LIBU_t(reader);
    }
    return new LIB_t(reader);
  }
  delete reader;
  return nullptr;
}

const std::string &MLib::GetLocation() const {
  return location_;
}

bool MLib::IsDirectory() const {
  return (IsFile() == false);
}

MLib *MLib::Parent() {
  return parent_;
}

MLib *MLib::Child(size_t i) {
  if (IsFile()) {
    return nullptr;
  }
  LoadChild();
  if (children_.size() <= i) {
    return nullptr;
  }
  return children_.at(i);
}

MLib *MLib::Child(const std::string &name) {
  if (IsFile()) {
    return nullptr;
  }
  LoadChild();
  for (const auto &p_child : children_) {
    if (p_child->GetName() == name) {
      return p_child;
    }
  }
  return nullptr;
}

void MLib::LoadChild() {
  if (IsFile() || children_.size() == GetChildNumber()) return;
  DoLoadChild();
  assert(children_.size() == GetChildNumber());
}

void MLib::AppendChild(MLib *child) {
  children_.push_back(child);
}

MLib *MLib::GetEntry(const std::string &path, size_t index) {
#ifdef _WINDOWS
  const size_t delim_pos = path.find_first_of('\\', index);
#else
  const size_t delim_pos = path.find_first_of('/', index);
#endif
  const std::string target_entry_name =
      (index == std::string::npos) ? "" :
      (path.substr(index, (delim_pos == std::string::npos) ? std::string::npos : delim_pos - index));
  const size_t sub_index =
      (delim_pos == std::string::npos) ? std::string::npos : delim_pos + 1;

  if (target_entry_name == "") {
    return this;
  }
  if (IsFile()) {
    if (target_entry_name == ".") {
      MLib *parent = Parent();
      if (parent == nullptr) return nullptr;
      return parent->GetEntry(path, sub_index);
    }
    if (target_entry_name == "..") {
      MLib *parent = Parent();
      if (parent == nullptr) return nullptr;
      parent = parent->Parent();
      if (parent == nullptr) return nullptr;
      return parent->GetEntry(path, sub_index);
    }
    return nullptr;
  }

  if (target_entry_name == ".") {
    return GetEntry(path, sub_index);
  }
  if (target_entry_name == "..") {
    return Parent()->GetEntry(path, sub_index);
  }

  MLib *child = Child(target_entry_name);
  if (child == nullptr) {
    return nullptr;
  }
  return child->GetEntry(path, sub_index);
}

MLib *MLib::GetEntry(const std::string &path) {
  std::string path_tmp(path);
#ifdef _WINDOWS
  std::replace(path_tmp.begin(), path_tmp.end(), '/', '\\');
#else
  std::replace(path_tmp.begin(), path_tmp.end(), '\\', '/');
#endif
#ifdef _WINDOWS
  if (path_tmp.back() == '\\') {
#else
  if (path_tmp.back() == '/') {
#endif
    path_tmp.pop_back();
  }
  return GetEntry(path_tmp, 0);
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
  MLib *entry = const_cast<MLib*>(this)->GetEntry(path);
  if (entry == nullptr) {
    std::cerr << path << ": invalid path." << std::endl;
    return;
  }
  if (entry->IsFile()) {
    std::cout << "- " << entry->GetName() << std::endl;
  } else {
    for (unsigned int i = 0; i < GetChildNumber(); ++i) {
      MLib *child = entry->Child(i);
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

LIB_t::LIB_t(LibReader *reader)
  : MLib(reader), offset_(0), file_size_(reader->GetSize()) {
  reader->Read(0, sizeof(hdr_), &hdr_);
}

LIB_t::LIB_t(LIB_t *parent, const LIBENTRY &entry_info)
  : MLib(parent), name_(entry_info.file_name),
    offset_(parent->offset_ + entry_info.offset),
    file_size_(entry_info.length) {
  Read(sizeof(hdr_), &hdr_);
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

void LIB_t::DoLoadChild() {
  const unsigned int num = hdr_.entry_count;
  entries_.assign(num, LIBENTRY());
  Seek(sizeof(LIBHDR), SEEK_SET);
  Read(sizeof(LIBENTRY) * num, &entries_[0]);
  for (unsigned int i = 0; i < num; ++i) {
    AppendChild(new LIB_t(this, entries_[i]));
  }
}

off_t LIB_t::GetFileBaseOffset() const {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBU_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBU_t::LIBU_t(LibReader *reader)
  : MLib(reader), offset_(0), file_size_(reader->GetSize()) {
  reader->Read(0, sizeof(hdr_), &hdr_);
}

LIBU_t::LIBU_t(LIBU_t *parent, const LIBUENTRY &entry_info)
  : MLib(parent), name_(),
    offset_(parent->offset_ + entry_info.offset),
    file_size_(entry_info.length) {
  char mb_name[32*5 + 1];
  utf16_to_utf8(entry_info.file_name, mb_name);
  name_.assign(mb_name);
  Read(sizeof(hdr_), &hdr_);
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

void LIBU_t::DoLoadChild() {
  const unsigned int num = hdr_.entry_count;
  entries_.assign(num, LIBUENTRY());
  Seek(sizeof(LIBUHDR), SEEK_SET);
  Read(sizeof(LIBUENTRY) * num, &entries_[0]);
  for (unsigned int i = 0; i < num; ++i) {
    AppendChild(new LIBU_t(this, entries_[i]));
  }
}

off_t LIBU_t::GetFileBaseOffset() const {
  return offset_;
}

////////////////////////////////////////////////////////////////////////
// LIBP_t Class Function Definitions
////////////////////////////////////////////////////////////////////////

LIBP_t::SharedObject::SharedObject(LibReader *reader) {

  size_t data_base_offset = reader->Read(0, sizeof(LIBPHDR), &hdr_);

  entries_.assign(hdr_.entry_count, LIBPENTRY());
  data_base_offset += reader->Read(data_base_offset, sizeof(LIBPENTRY) * hdr_.entry_count, &entries_[0]);

  file_offsets_.assign(hdr_.file_count, 0);
  data_base_offset += reader->Read(data_base_offset, sizeof(unsigned int) * hdr_.file_count, &file_offsets_[0]);

  data_base_offset_ = (data_base_offset + 1023) & ~1023;
}

LIBP_t::LIBP_t(LibReader *reader)
  : MLib(reader), shobj_(new SharedObject(reader)), entry_index_(0), name_(shobj_->entries_.at(0).file_name) {}

LIBP_t::LIBP_t(const boost::shared_ptr<SharedObject> &shobj, LIBP_t* parent, unsigned int entry_index)
  : MLib(parent), shobj_(shobj), entry_index_(entry_index), name_(shobj_->entries_.at(entry_index).file_name) {}

const std::string &LIBP_t::GetName() const {
  return name_;
}

bool LIBP_t::IsFile() const {
  return (shobj_->entries_[entry_index_].flags & LIBPENTRY::kFlagFile);
}

unsigned int LIBP_t::GetFileSize() const {
  if (IsDirectory()) { return 0; }
  return shobj_->entries_.at(entry_index_).length;
}

unsigned int LIBP_t::GetChildNumber() const {
  if (IsFile()) { return 0; }
  return shobj_->entries_.at(entry_index_).length;
}

void LIBP_t::DoLoadChild() {
  const unsigned int num = GetChildNumber();
  for (unsigned int i = 0; i < num; ++i) {
    AppendChild(new LIBP_t(shobj_, this, shobj_->entries_.at(entry_index_).offset_index + i));
  }
}

off_t LIBP_t::GetFileBaseOffset() const {
  if (IsDirectory()) { return -1; }
  off_t base_offset = shobj_->file_offsets_.at(shobj_->entries_.at(entry_index_).offset_index);
  base_offset *= 1024;
  base_offset += shobj_->data_base_offset_;
  return base_offset;
}

////////////////////////////////////////////////////////////////////////
// Key Load Functions
////////////////////////////////////////////////////////////////////////

bool LoadKeyInfo(const std::string &csv) {
  std::ifstream ifs(csv);
  if (ifs.is_open() == false) return false;
  std::string l, product, rel_date, key, type;
  ifs >> l; // "PRODUCT_NAME,RELEASE_DATE,KEY_STRING,CIPHER_TYPE
  KeyInfo info;
  while (ifs.eof() == false) {
    ifs >> l;
    std::istringstream iss(l);
    std::getline(iss, product, ',');
    std::getline(iss, rel_date, ',');
    std::getline(iss, key, ',');
    std::getline(iss, type, ',');
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
    case 0:
      std::cout << "Plain";
      break;
    case 1:
      std::cout << "Camellia128";
      break;
    case 2:
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
