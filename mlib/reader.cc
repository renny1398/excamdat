/* reader.cc (updated on 2016/12/29)
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

#include <memory>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cassert>
#include "reader.h"

namespace {

struct KeyInfo {
  std::string product_name;
  std::string release_date;
  std::string key;
  unsigned char raw_key[16];
  friend bool operator<(const KeyInfo &lhs, const KeyInfo &rhs) {
    if (lhs.release_date < rhs.release_date) return true;
    else if (lhs.release_date > rhs.release_date) return false;
    else if (lhs.product_name < rhs.product_name) return true;
    else return false;
  }
};
std::set<KeyInfo> key_info_;

bool GetKeyTable(const std::string &product, KEY_TABLE_TYPE key_table) {
  for (auto it = key_info_.cbegin(); it != key_info_.cend(); ++it) {
    if (product == it->product_name) {
      Camellia_Ekeygen(128, it->raw_key, key_table);
      return true;
    }
  }
  return false;
}

bool IsAsciiKey(const unsigned char raw_key[]) {
  for (auto i = 0; i < 16; ++i) {
    const auto &c = raw_key[i];
    if ( ('!' <= c && c <= '+') || ('-' <= c && c <= '~') ) continue;
    else return false;
  }
  return true;
}

} // namespace

namespace mlib {

const size_t LibReader::kCacheSize = 65536;

LibReader::LoadInfo::LoadInfo()
  : mutex_(), cond_(),
    load_cache_no_(-1), load_page_no_(SIZE_MAX), load_size_(0),
    terminate_(false) {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);
}

LibReader::LoadInfo::~LoadInfo() {
  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&mutex_);
}

////////////////////////////////////////////////////////////////////////
// LibReader Class Function Definitions
////////////////////////////////////////////////////////////////////////

LibReader::LibReader(int fd)
  : fd_(fd), file_size_(0), cache_(), page_no_(), cache_length_(),
    pt_(), load_info_(), is_verbose_(false) {
  struct stat st;
  ::fstat(fd, &st);
  file_size_ = st.st_size;

  cache_[0] = new char[kCacheSize];
  cache_[1] = new char[kCacheSize];
  page_no_[0] = SIZE_MAX;
  page_no_[1] = SIZE_MAX;
  cache_length_[0] = 0;
  cache_length_[1] = 0;

  pthread_create(&pt_, NULL, &LoadEntry, this);
}

LibReader::~LibReader() {
  pthread_mutex_lock(&load_info_.mutex_);
  load_info_.load_cache_no_ = -1;
  load_info_.terminate_ = true;
  pthread_cond_broadcast(&load_info_.cond_);
  pthread_mutex_unlock(&load_info_.mutex_);
  pthread_cancel(pt_);
  pthread_join(pt_, NULL);

  delete [] cache_[1];
  delete [] cache_[0];
}

bool LibReader::IsVerbose() const {
  return is_verbose_;
}

void LibReader::Verbose(bool b) {
  is_verbose_ = b;
}

int LibReader::fd() const {
  return fd_;
}

size_t LibReader::GetSize() const {
  return file_size_;
}

void *LibReader::LoadEntry(void *args) {
  LibReader *reader = reinterpret_cast<LibReader *>(args);
  LibReader::LoadInfo &load_info = reader->load_info_;

  volatile int load_cache_no;
  size_t load_page_no;
  size_t load_size;
  off_t file_pointer = ::lseek(reader->fd_, 0, SEEK_CUR);
  off_t offset;

  do {
    pthread_mutex_lock(&load_info.mutex_);
    while (load_info.load_cache_no_ == -1) {
      if (load_info.terminate_ == true) {
        break;
      }
      pthread_cond_wait(&load_info.cond_, &load_info.mutex_);
    }
    if (load_info.terminate_ == true) {
      pthread_mutex_unlock(&load_info.mutex_);
      break;
    }
    assert(load_info.load_cache_no_ != -1);
    load_cache_no = load_info.load_cache_no_;
    load_page_no = load_info.load_page_no_;
    load_size = load_info.load_size_;
    pthread_mutex_unlock(&load_info.mutex_);

    assert(load_cache_no != -1);

    offset = load_page_no * reader->kCacheSize;
    if (offset != file_pointer) {
      if (reader->IsVerbose()) {
        std::printf("Move the file pointer from %lld to %lld.\n", file_pointer, offset);
      }
      file_pointer = offset;
      ::lseek(reader->fd_, offset, SEEK_SET);
    }
    if (reader->IsVerbose()) {
      std::printf("Load data 0x%08llx-0x%08llx into cache[%d]. (Page Number: %ld, Size: %ld)\n",
                  offset, offset + load_size - 1,
                  load_cache_no, load_page_no, load_size);
    }
    ssize_t read_size = ::read(reader->fd_, reader->cache_[load_cache_no], load_size);
    if (0 <= read_size) {
      file_pointer += read_size;
    }

    pthread_mutex_lock(&load_info.mutex_);
    load_info.load_cache_no_ = -1;
    load_info.load_size_ = read_size;
    pthread_cond_broadcast(&load_info.cond_);
    pthread_mutex_unlock(&load_info.mutex_);
  } while (true);
  return NULL;
}

bool LibReader::LoadAsync(int cache_no, size_t page_no, size_t length) {
  pthread_mutex_lock(&load_info_.mutex_);
  load_info_.load_cache_no_ = cache_no;
  load_info_.load_page_no_ = page_no;
  load_info_.load_size_ = length;
  pthread_cond_broadcast(&load_info_.cond_);
  pthread_mutex_unlock(&load_info_.mutex_);
  return true;
}

size_t LibReader::WaitLoad() {
  pthread_mutex_lock(&load_info_.mutex_);
  while (load_info_.load_cache_no_ != -1) {
    pthread_cond_wait(&load_info_.cond_, &load_info_.mutex_);
  }
  size_t load_size = load_info_.load_size_;
  pthread_mutex_unlock(&load_info_.mutex_);
  return load_size;
}

size_t LibReader::Read(size_t offset, size_t length, void* dest) {
  if (length == 0) return 0;
  if (file_size_ <= offset) return 0;
  if (file_size_ < offset + length) {
    length = file_size_ - offset;
  }

  size_t read_page_no = offset / kCacheSize;
  size_t read_cache_offset = offset % kCacheSize;
  const size_t read_last_page_no = (offset + length - 1) / kCacheSize;

  int load_cache_no = -1;
  int read_cache_no = -1;
  size_t load_page_no = read_page_no;
  size_t load_remain_count = (read_last_page_no + 1) - read_page_no;

  if (load_page_no == page_no_[0]) {
    if (IsVerbose()) {
      std::printf("Hit cache[0]! (Page Number: %ld)\n", page_no_[0]);
    }
    ++load_page_no;
    --load_remain_count;
    load_cache_no = load_remain_count == 0 ? -1 : 1;
    read_cache_no = 0;
  } else if (load_page_no == page_no_[1]) {
    if (IsVerbose()) {
      std::printf("Hit cache[1]! (Page Number: %ld)\n", page_no_[1]);
    }
    ++load_page_no;
    --load_remain_count;
    load_cache_no = load_remain_count == 0 ? -1 : 0;
    read_cache_no = 1;
  } else {
    if (IsVerbose()) {
      std::printf("Not hit cache. (Page Number: %ld)\n", load_page_no);
    }
    load_cache_no = (load_page_no + 1 == page_no_[0]) ? 1 : 0;
    read_cache_no = -1;
  }

  size_t total_read_size = 0;
  size_t load_size = 0;
  size_t read_size = 0;

  do {
    if (load_cache_no != -1) {
      load_size = std::min(kCacheSize, file_size_ - (load_page_no * kCacheSize));
      LoadAsync(load_cache_no, load_page_no, load_size);
    }
    if (read_cache_no != -1) {
      read_size = std::min(cache_length_[read_cache_no] - read_cache_offset, length - total_read_size);
      if (IsVerbose()) {
        std::printf("Read data 0x%08lx-0x%08lx from cache[%d]. (Page Number: %ld, Size: %ld)\n",
                    read_page_no*kCacheSize + read_cache_offset,
                    read_page_no*kCacheSize + read_cache_offset + read_size - 1,
                    read_cache_no, read_page_no, read_size);
      }
      MemoryCopy(cache_[read_cache_no], read_page_no, read_cache_offset, read_size,
                 reinterpret_cast<unsigned char*>(dest) + total_read_size);
      total_read_size += read_size;
      if (length <= total_read_size || load_cache_no == -1) break;
      // read_cache_no = (read_cache_no == 0) ? 1 : 0;
      ++read_page_no;
      read_cache_offset = 0;
    }
    if (load_cache_no != -1) {
      cache_length_[load_cache_no] = WaitLoad();
      page_no_[load_cache_no] = load_page_no;
      // read_page_no = load_page_no;
      read_cache_no = load_cache_no;
      ++load_page_no;
      --load_remain_count;
      // std::printf("Loaded Remain: %ld\n", load_remain_count);
      if (load_remain_count == 0) {
        load_cache_no = -1;
      } else {
        load_cache_no = (load_cache_no == 0) ? 1 : 0;
      }
    }
  } while (true);

  return total_read_size;
}

////////////////////////////////////////////////////////////////////////
// UnencryptedLibReader Class Function Definitions
////////////////////////////////////////////////////////////////////////

UnencryptedLibReader::UnencryptedLibReader(int fd) : LibReader(fd) {}

size_t UnencryptedLibReader::MemoryCopy(char *cache, size_t /*page_no*/, size_t offset, size_t length, void *dest) {
  ::memcpy(dest, cache + offset, length);
  return length;
}

////////////////////////////////////////////////////////////////////////
// EncryptedLibReader Class Function Definitions
////////////////////////////////////////////////////////////////////////

EncryptedLibReader::EncryptedLibReader(int fd, const std::string &product)
  : LibReader(fd),
    cache_(new unsigned char[kCacheSize]), cache_offset_(SIZE_MAX), cache_length_(0) {
  if (GetKeyTable(product, key_table_) == false) {
    std::cerr << "ERROR: the specified product '" << product << "' is not supported." << std::endl;
  }
}

EncryptedLibReader::~EncryptedLibReader() {
  delete [] cache_;
}

inline unsigned int rotl(unsigned int data, unsigned int bits) {
 return (data << bits) | (data >> (32 - bits));
}

inline unsigned int rotr(unsigned int data, unsigned int bits) {
 return (data >> bits) | (data << (32 - bits));
}

size_t EncryptedLibReader::MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void* dest) {
  size_t file_offset = page_no * kCacheSize + offset;
  size_t file_aligned_offset = file_offset & ~0x0f;
  size_t aligned_offset = file_aligned_offset - (page_no * kCacheSize);
  size_t aligned_length = (length + (file_offset & 0x0f) + 0x0f) & ~0x0f;
  assert(aligned_offset + aligned_length <= kCacheSize);

  if (file_aligned_offset != cache_offset_ || aligned_length != cache_length_) {
    cache_offset_ = file_aligned_offset;
    cache_length_ = aligned_length;

    for (size_t i = 0; i < aligned_length; i += 16) {
      unsigned char block_tmp[16];
      int roll_bits = ( ((file_aligned_offset + i) >> 4) & 0x0f ) + 16;
      unsigned int *p = reinterpret_cast<unsigned int *>(cache + aligned_offset + i);
      unsigned int *q = reinterpret_cast<unsigned int *>(block_tmp);

      q[0] = rotl(p[0], roll_bits);
      q[1] = rotr(p[1], roll_bits);
      q[2] = rotl(p[2], roll_bits);
      q[3] = rotr(p[3], roll_bits);

      Camellia_DecryptBlock(128, block_tmp, key_table_, cache_ + i);
    }
  }

  ::memcpy(dest, cache_ + (offset & 0x0f), length);

  return length;
}

bool EncryptedLibReader::LoadKeyInfo(const std::string &csv) {
  if (key_info_.empty() == false) return true;
  std::ifstream ifs(csv);
  if (ifs.is_open() == false) {
    std::cerr << "ERROR: cannot open '" << csv << "'." << std::endl;
    return false;
  }
  std::string l;
  ifs >> l;
  while (ifs.eof() == false) {
    ifs >> l;
    KeyInfo key_info;
    std::istringstream iss(l);
    std::getline(iss, key_info.product_name, ',');
    std::getline(iss, key_info.release_date, ',');
    std::getline(iss, key_info.key, ',');

    if (key_info.key.size() != 16) {
      if (key_info.key.size() != 32) continue;
      auto it = key_info.key.cbegin();
      const auto it_end = key_info.key.cend();
      for (; it != it_end; ++it) {
        int c = *it;
        if ( ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') ||
             ('a' <= c && c <= 'f') ) continue;
        else break;
      }
      if (it != it_end) continue;
      for (int i = 0; i < 16; ++i) {
        std::string s(key_info.key.substr(2*i, 2));
        key_info.raw_key[i] = static_cast<unsigned char>(std::stoi(s, NULL, 16));
      }
      if (IsAsciiKey(key_info.raw_key)) {
        key_info.key.assign(reinterpret_cast<char *>(key_info.raw_key), 16);
      }
    } else {
      ::memcpy(key_info.raw_key, &key_info.key[0], 16);
    }
    key_info_.insert(key_info);
  }
  return true;
}

void EncryptedLibReader::PrintKeyInfo() {
  std::cout << "PRODUCT_NAME  RELEASE_DATE  KEY\n"
            << "------------- ------------- --------------------------------------------------\n";
  std::cout.setf(std::ios::hex, std::ios::basefield);
  for (auto it = key_info_.cbegin(); it != key_info_.cend(); ++it) {
    char fix_product_name[15];
    size_t fix_product_name_len = std::min(it->product_name.size(), static_cast<size_t>(14));
    ::memcpy(fix_product_name, &it->product_name[0], fix_product_name_len);
    fix_product_name[fix_product_name_len] = '\0';
    std::cout << std::left << std::setfill(' ') << std::setw(14) << fix_product_name;
    char fix_release_date[15];
    size_t fix_release_date_len = std::min(it->release_date.size(), static_cast<size_t>(14));
    if (fix_release_date_len == 8) {
      // convert 'YYYYMMDD' into 'YYYY/MM/DD'
      ::memcpy(&fix_release_date[0], &it->release_date[0], 4);
      fix_release_date[4] = '/';
      ::memcpy(&fix_release_date[5], &it->release_date[4], 2);
      fix_release_date[7] = '/';
      ::memcpy(&fix_release_date[8], &it->release_date[6], 2);
      fix_release_date_len = 10;
    } else {
      ::memcpy(fix_release_date, &it->release_date[0], fix_release_date_len);
    }
    fix_release_date[fix_release_date_len] = '\0';
    std::cout << std::left << std::setfill(' ') << std::setw(14) << fix_release_date;
    if (IsAsciiKey(it->raw_key) == false) {
      std::cout << "{ ";
      const uint32_t *p = reinterpret_cast<const uint32_t *>(it->raw_key);
      for (int i = 0; ; ) {
        std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << p[i];
        ++i;
        if (4 <= i) {
          std::cout << " }\n";
          break;
        }
        std::cout << ", ";
      }
    } else {
      std::cout << '"' << it->key << "\"\n";
    }
  }
  std::cout << std::endl;
  std::cout.setf(std::ios::dec, std::ios::basefield);
}

} // namespace mlib
