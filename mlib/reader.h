#pragma once

/* reader.h (updated on 2018/04/18)
 * Copyright (C) 2016-2018 renny1398.
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
#include <string>
#include "camellia.h"

namespace mlib {

// Interface
class Reader {
public:
  virtual ~Reader() = default;
  bool IsVerbose() const { return verbose_; }
  void Verbose(bool verbose = true) { verbose_ = verbose; }

  virtual size_t GetSize() const = 0;
  virtual size_t Read(off_t offset, size_t length, void *dest);  

protected:
  virtual std::istream *istream() = 0;
  static bool verbose_;
};

class streambuf_base : public std::streambuf {
public:
  streambuf_base();
  virtual ~streambuf_base();

  streambuf_base *open(const std::string &filename);
  bool is_open() const;
  streambuf_base *close();

  size_t size() const;

protected:
  streambuf_base *setbuf(char *s, std::streamsize n) override;
  std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                         std::ios_base::openmode which) override;
  std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override;
  std::streamsize xsgetn(char *s, std::streamsize n) override;
  int underflow() override;
  int uflow() override;
  int overflow(int c) override;
  virtual void rewrite_buffer(off_t pos, char *buf, std::streamsize n) = 0;
private:
  off_t calculate_pos();

  static const unsigned int kBufferSize = 4096;
  char buf_[kBufferSize];
  int fd_;
  size_t file_size_;
};

class camelliabuf : public streambuf_base {
public:
  camelliabuf(const unsigned char key_string[16]) : streambuf_base() {
    Camellia_Ekeygen(128, key_string, key_table_);
  }
protected:
  void rewrite_buffer(off_t pos, char *buf, std::streamsize n) override;
private:
  KEY_TABLE_TYPE key_table_;
};

class crypto2buf : public streambuf_base {
public:
  crypto2buf(const unsigned char key_string[16]) : streambuf_base() {
    ::memcpy(key_, key_string, 16);
  }
protected:
  void rewrite_buffer(off_t pos, char *buf, std::streamsize n) override;
private:
  unsigned int key_[4];
};

class PlainReader : public Reader {
public:
  PlainReader(const std::string &filename);
  size_t GetSize() const override;
protected:
  std::istream *istream() override;
private:
  std::ifstream input_stream_;
  size_t file_size_;
};

class CamelliaDecrypter : public Reader {
public:
  CamelliaDecrypter(const std::string &filename, const unsigned char key_string[16]);
  // static bool LoadKeyInfo(const std::string &csv);
  // static void PrintKeyTable(const KEY_TABLE_TYPE key_table);
  size_t GetSize() const override;
protected:
  std::istream *istream() override;
private:
  camelliabuf stream_buf_;
  std::istream input_stream_;
  size_t file_size_;
};

class SecondCryptoDecrypter : public Reader {
public:
  SecondCryptoDecrypter(const std::string &filename, const unsigned char key_string[16]);
  // static bool LoadKeyInfo(const std::string &csv);
  size_t GetSize() const override;
protected:
  std::istream *istream() override;
private:
  crypto2buf stream_buf_;
  std::istream input_stream_;
  size_t file_size_;
};

} // namespace mlib