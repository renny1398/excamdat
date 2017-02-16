/* reader.cc (updated on 2017/02/16)
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

#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <cassert>
#include "reader.h"

namespace {

inline unsigned int rotl(unsigned int data, unsigned int bits) {
 return (data << bits) | (data >> (32 - bits));
}

inline unsigned int rotr(unsigned int data, unsigned int bits) {
 return (data >> bits) | (data << (32 - bits));
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

EncryptedLibReader::EncryptedLibReader(int fd, const unsigned char key_string[16])
  : LibReader(fd),
    cache_(new unsigned char[kCacheSize]), cache_offset_(SIZE_MAX), cache_length_(0) {
  Camellia_Ekeygen(128, key_string, key_table_);
}

EncryptedLibReader::~EncryptedLibReader() {
  delete [] cache_;
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

    unsigned char block_tmp[16];
    const unsigned int *p = reinterpret_cast<unsigned int *>(cache + aligned_offset);
    unsigned int *const q = reinterpret_cast<unsigned int *>(block_tmp);
    for (size_t i = 0; i < aligned_length; i += 16) {
      int roll_bits = ( ((file_aligned_offset + i) >> 4) & 0x0f ) + 16;
      q[0] = rotl(p[0], roll_bits);
      q[1] = rotr(p[1], roll_bits);
      q[2] = rotl(p[2], roll_bits);
      q[3] = rotr(p[3], roll_bits);
      Camellia_DecryptBlock(128, block_tmp, key_table_, cache_ + i);
      p += 4;
    }
  }

  ::memcpy(dest, cache_ + (offset & 0x0f), length);

  return length;
}

////////////////////////////////////////////////////////////////////////
// EncryptedLibReader2 Class Function Definitions
////////////////////////////////////////////////////////////////////////

EncryptedLibReader2::EncryptedLibReader2(int fd, const unsigned char key_string[16])
  : LibReader(fd),
    key_{0}, cache_(new char[kCacheSize]), cache_offset_(SIZE_MAX), cache_length_(0) {
  ::memcpy(key_, key_string, 16);
}

EncryptedLibReader2::~EncryptedLibReader2() {
  delete [] cache_;
}

size_t EncryptedLibReader2::MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void* dest) {

  static const unsigned char rotate_table[0x20] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09,
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x1F, 0x04, 0x0C, 0x14, 0x1C
  };

  size_t file_offset = page_no * kCacheSize + offset;
  size_t file_aligned_offset = file_offset & ~0x0f;
  size_t aligned_offset = file_aligned_offset - (page_no * kCacheSize);
  size_t aligned_length = (length + (file_offset & 0x0f) + 0x0f) & ~0x0f;
  assert(aligned_offset + aligned_length <= kCacheSize);

  if (file_aligned_offset != cache_offset_ || aligned_length != cache_length_) {
    cache_offset_ = file_aligned_offset;
    cache_length_ = aligned_length;
    for (size_t i = 0; i < aligned_length; i += 0x10) {
      const char *p = cache + aligned_offset + i;
      char *q = cache_ + i;
      const int d = (aligned_offset + i) & 0xf;
      const char p_d = p[d];
      for (int j = 0; j < 0x10; ++j) {
        char c = p[j];
        if (d != j) {
          c ^= p_d;
        }
        q[j] = c;
      }
      const size_t block_ofs = (file_aligned_offset + i) >> 4;
      unsigned int *q_dw = reinterpret_cast<unsigned int *>(q);
      const char roll1 = rotate_table[(block_ofs + 0x00) & 0x1f];
      const char roll2 = rotate_table[(block_ofs + 0x0c) & 0x1f];
      q_dw[0] = rotr(rotr(key_[0], roll1) ^ q_dw[0], roll2);
      const char roll3 = rotate_table[(block_ofs + 0x03) & 0x1f];
      const char roll4 = rotate_table[(block_ofs + 0x0f) & 0x1f];
      q_dw[1] = rotl(rotl(key_[1], roll3) ^ q_dw[1], roll4);
      const char roll5 = rotate_table[(block_ofs + 0x06) & 0x1f];
      const char roll6 = rotate_table[(block_ofs - 0x0e) & 0x1f];
      q_dw[2] = rotr(rotr(key_[2], roll5) ^ q_dw[2], roll6);
      const char roll7 = rotate_table[(block_ofs + 0x09) & 0x1f];
      const char roll8 = rotate_table[(block_ofs - 0x0b) & 0x1f];
      q_dw[3] = rotl(rotl(key_[3], roll7) ^ q_dw[3], roll8);
    }
  }

  ::memcpy(dest, cache_ + (offset & 0x0f), length);

  return length;
}

} // namespace mlib
