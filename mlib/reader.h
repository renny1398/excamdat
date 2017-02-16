#pragma once

/* reader.h (updated on 2017/02/16)
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

#include <stdint.h>
#include <pthread.h>
#include <string>
#include <set>
#include "camellia.h"

namespace mlib {

class LibReader {
public:
  explicit LibReader(int fd);
  virtual ~LibReader();

  bool IsVerbose() const;
  void Verbose(bool b = true);

  int fd() const;
  size_t GetSize() const;

  size_t Read(size_t offset, size_t length, void *dest);

protected:
  virtual size_t MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void *dest) = 0;

  static const size_t kCacheSize;

private:
  bool LoadAsync(int cache_no, size_t page_no, size_t length);
  size_t WaitLoad();

  static void *LoadEntry(void *args);

  int fd_;
  size_t file_size_;

  char *cache_[2];
  size_t page_no_[2];
  size_t cache_length_[2];

  struct LoadInfo {
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    int load_cache_no_;
    size_t load_page_no_;
    size_t load_size_;
    bool terminate_;
    LoadInfo();
    ~LoadInfo();
  };
  pthread_t pt_;
  LoadInfo load_info_;

  bool is_verbose_;
};

class UnencryptedLibReader : public LibReader {
public:
  UnencryptedLibReader(int fd);
protected:
  size_t MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void *dest) override;
};

class EncryptedLibReader : public LibReader {
public:
  EncryptedLibReader(int fd, const unsigned char key_string[16]);
  ~EncryptedLibReader() override;
  static bool LoadKeyInfo(const std::string &csv);
  static void PrintKeyTable(const KEY_TABLE_TYPE key_table);
protected:
  size_t MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void *dest) override;
private:
  KEY_TABLE_TYPE key_table_;
  unsigned char *cache_;
  size_t cache_offset_;
  size_t cache_length_;
};

class EncryptedLibReader2 : public LibReader {
public:
  EncryptedLibReader2(int fd, const unsigned char key_string[16]);
  ~EncryptedLibReader2() override;
  static bool LoadKeyInfo(const std::string &csv);
protected:
  size_t MemoryCopy(char *cache, size_t page_no, size_t offset, size_t length, void *dest) override;
private:
  unsigned int key_[4];
  char *cache_;
  size_t cache_offset_;
  size_t cache_length_;
};

}   // namespace mlib
