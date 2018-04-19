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

// require for low-level file control
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <algorithm>
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

bool Reader::verbose_ = false;

streambuf_base::streambuf_base()
    : std::streambuf(), fd_(-1), file_size_(0UL) {
  static_assert(kBufferSize % 16 == 0, "invalid buffer size");
  // setg(buff_, nullptr, buff_ + sizeof(buff_));
  setg(nullptr, nullptr, nullptr);
  setp(nullptr, nullptr);
}

streambuf_base::~streambuf_base() {
  close();
}

streambuf_base *streambuf_base::open(const std::string &filename) {
  int fd = ::open(filename.c_str(), O_RDONLY);
  if (fd == -1) return nullptr;
  if (fd_ != -1) close();
  struct stat st;
  ::fstat(fd, &st);
  fd_ = fd;
  file_size_ = st.st_size;
  return this;  
}

bool streambuf_base::is_open() const {
  return (fd_ != -1);
}

streambuf_base *streambuf_base::close() {
  if (fd_ == -1) return nullptr;
  ::close(fd_);
  file_size_ = 0UL;
  return this;
}

size_t streambuf_base::size() const {
  return file_size_;
}

streambuf_base *streambuf_base::setbuf(char *, std::streamsize) {
  return this;
}

std::streampos streambuf_base::seekoff(std::streamoff off,
                                       std::ios_base::seekdir way,
                                       std::ios_base::openmode which) {
  if ( (which & std::ios_base::in) != std::ios_base::in ) {
    return std::streampos(std::streamoff(-1));
  }
  const auto prev_pos = calculate_pos();
  // convert 'off' into the absolute position
  switch (way) {
  case std::ios_base::beg:
    break;
  case std::ios_base::cur:
    off += prev_pos;
    break;
  case std::ios_base::end:
    off += file_size_;
    break;
  default:
    return std::streampos(std::streamoff(-1));
  }
  const auto new_pos = ::lseek(fd_, off, SEEK_SET);
  if (new_pos == -1) {
    setg(nullptr, nullptr, nullptr);
    return std::streampos(new_pos);
  }
  if (new_pos == prev_pos) {
#if 0
    std::cerr << "mlib::streambuf: WARNING: the file position doesn't need to be changed."
              << " (pos = " << new_pos << ")" << std::endl;
#endif
    return std::streampos(new_pos);
  }
#if 0
  std::cerr << "mlib::streambuf: seeked to " << new_pos << "." << std::endl;
#endif
  const auto prev_gindex = prev_pos % kBufferSize;
  const auto prev_aligned_pos = prev_pos - prev_gindex;
  const auto new_gindex = new_pos % kBufferSize;
  const auto new_aligned_pos = new_pos - new_gindex; 
  if (prev_aligned_pos != new_aligned_pos) {
    setg(nullptr, nullptr, nullptr);
  } else {
    setg(eback(), eback() + new_gindex, egptr());
  }
  return std::streampos(new_pos);
}

std::streampos streambuf_base::seekpos(std::streampos pos,
                                       std::ios_base::openmode which) {
  return seekoff(pos, std::ios_base::beg, which);
}

std::streamsize streambuf_base::xsgetn(char *s, std::streamsize n) {
  if (fd_ == -1 || s == nullptr) return 0;
  std::streamsize read_bytes = 0;
  while (read_bytes < n) {
    if (gptr() == nullptr || gptr() >= egptr()) {
      int c = underflow();
      if (c == EOF) break;
    }
    assert(gptr() != nullptr);
    auto to_read_bytes = std::min(egptr() - gptr(), n - read_bytes);
#if 0
    std::cerr << "mlib::streambuf: memcpy(" << (void*)s <<  ", "
              << (void*)gptr() << ", " << to_read_bytes << ")." << std::endl;
#endif
    ::memcpy(s + read_bytes, gptr(), to_read_bytes);
    gbump(to_read_bytes);
    read_bytes += to_read_bytes;
  }
  return read_bytes;
}

int streambuf_base::underflow() {
  auto pos = calculate_pos();
  if (pos == -1 || pos >= static_cast<off_t>(file_size_)) {
    return EOF;
  }
#if 0
  std::cerr << "mlib::streambuf starts underflow()."
            << " (eback = " << (void*)eback() << ", gptr = " << (void*)gptr()
            << ", egptr = " << (void*)egptr() << ")" << std::endl;
  std::cerr << "mlib::streambuf: calculated pos: the value is " << pos << std::endl;
#endif
  const auto gindex = pos % kBufferSize;
  off_t gpos = pos - gindex;
  gpos = ::lseek(fd_, gpos, SEEK_SET);
  if (gpos == -1) {
    std::cerr << "mlib::streambuf::underflow(): lseek error." << std::endl;
    return EOF;
  }
  const auto read_bytes = ::read(fd_, buf_, kBufferSize);
#if 0
  if (read_bytes < kBufferSize) {
    std::cerr << "mlib::streambuf::underflow(): reach EOF. (read "
              << read_bytes << " bytes from " << gpos << ")" << std::endl;
  }
#endif
  if (read_bytes == -1 || gindex >= read_bytes) {
    std::cerr << "mlib::streambuf::underflow(): unknown error." << std::endl;
    return EOF;
  }
  ::lseek(fd_, gpos, SEEK_SET);
  setg(buf_, buf_ + gindex, buf_ + read_bytes);
  rewrite_buffer(gpos, eback(), read_bytes);
#if 0
  std::cerr << "mlib::streambuf ends underflow() successfully."
            << " (buffered size = " << read_bytes << ", gindex = " << gindex << ")" << std::endl;
#endif
  // convert CHAR(-1) into not INT(-1) (means EOF) but INT(255)
  return *reinterpret_cast<unsigned char *>(gptr());
}

int streambuf_base::uflow() {
  int c = underflow();
  if (c == EOF) return EOF;
  gbump(1);
  return c;
}

int streambuf_base::overflow(int) {
  return EOF;
}

off_t streambuf_base::calculate_pos() {
  if (fd_ == -1) return -1;
  auto fd_pos = ::lseek(fd_, 0, SEEK_CUR);
  if (fd_pos == -1) return -1;
  auto pos = (gptr() == nullptr) ? fd_pos :
             (fd_pos - (fd_pos % kBufferSize)) + (gptr() - eback());
  return pos;
}

void camelliabuf::rewrite_buffer(off_t pos, char *buf, std::streamsize n) {
  assert(pos % 16 == 0);
  assert(n % 16 == 0);
  auto block_pos = pos >> 4; // (pos / 16)
  auto block_count = n >> 4; // (n / 16)
  unsigned char cipher[16];
  unsigned char *plaintext = reinterpret_cast<unsigned char *>(buf);
  const unsigned int *p = reinterpret_cast<const unsigned int *>(buf);
  unsigned int *const q = reinterpret_cast<unsigned int *>(cipher);
  int roll_bits = (block_pos & 0x0f) | 0x10;
  for (decltype(block_count) i = 0; i < block_count; ++i) {
    q[0] = rotl(p[0], roll_bits);
    q[1] = rotr(p[1], roll_bits);
    q[2] = rotl(p[2], roll_bits);
    q[3] = rotr(p[3], roll_bits);
    Camellia_DecryptBlock(128, cipher, key_table_, plaintext);
    p += 4;
    plaintext += 16;
    roll_bits = ((roll_bits + 1) & 0x0f) | 0x10;
  }
}

void crypto2buf::rewrite_buffer(off_t pos, char *buf, std::streamsize n) {
  assert(pos % 16 == 0);
  assert(n % 16 == 0);

  static const unsigned char rotate_table[0x20] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09,
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x1F, 0x04, 0x0C, 0x14, 0x1C
  };

  auto block_pos = pos >> 4;
  const auto block_epos = block_pos + (n >> 4);
  char *p = buf;
  unsigned int *dwp = reinterpret_cast<unsigned int *>(buf);
  const int i = pos & 0xf;  // always 0
  while (block_pos < block_epos) {
    const char p_i = p[i];
    for (int j = 0; j < 0x10; ++j) {
      if (j != i) { p[j] ^= p_i; }
    }
    const char roll1 = rotate_table[(block_pos + 0x00) & 0x1f];
    const char roll2 = rotate_table[(block_pos + 0x0c) & 0x1f];
    dwp[0] = rotr(rotr(key_[0], roll1) ^ dwp[0], roll2);
    const char roll3 = rotate_table[(block_pos + 0x03) & 0x1f];
    const char roll4 = rotate_table[(block_pos + 0x0f) & 0x1f];
    dwp[1] = rotl(rotl(key_[1], roll3) ^ dwp[1], roll4);
    const char roll5 = rotate_table[(block_pos + 0x06) & 0x1f];
    const char roll6 = rotate_table[(block_pos - 0x0e) & 0x1f];
    dwp[2] = rotr(rotr(key_[2], roll5) ^ dwp[2], roll6);
    const char roll7 = rotate_table[(block_pos + 0x09) & 0x1f];
    const char roll8 = rotate_table[(block_pos - 0x0b) & 0x1f];
    dwp[3] = rotl(rotl(key_[3], roll7) ^ dwp[3], roll8);
    p += 16;
    dwp += 4;
    ++block_pos;
  }
}

size_t Reader::Read(off_t offset, size_t length, void *dest) {
  const auto file_size = GetSize();
  if (static_cast<size_t>(offset) >= file_size) return 0;
  std::istream *input_stream = istream();
  if (input_stream == nullptr) return 0;
  input_stream->seekg(offset);
  if (input_stream->rdstate() & std::ios_base::failbit) {
    std::cerr << "mlib::streambuf: failed to seekg(" << offset << ")." << std::endl;
    return 0UL;
  }
  char *s = static_cast<char *>(dest);
#if 0
  std::cerr << "mlib::streambuf: read " << length << " bytes from offset "
            << offset << "." << std::endl; 
#endif
  input_stream->read(s, length);
  if (input_stream->rdstate() & std::ios_base::failbit) {
    if ( (input_stream->rdstate() & std::ios_base::eofbit) == 0 ) {
      std::cerr << "mlib::streambuf: failed to read." << std::endl;
      return 0;
    } else {
      const size_t read_bytes = file_size - offset;
#if 0
      std::cerr << "mlib::streambuf: reach EOF (read "
                << read_bytes << " bytes)." << std::endl;
#endif
      return read_bytes;
    }
  }
  const size_t read_bytes = input_stream->tellg() - offset;
#if 0
  std::cerr << "mlib::streambuf: read " << read_bytes << " bytes successfully." << std::endl;
#endif
  return read_bytes;
}

PlainReader::PlainReader(const std::string &filename)
  : input_stream_(filename) {
  if (input_stream_.is_open() == false) {
    file_size_ = 0;
  } else {
    input_stream_.seekg(0, std::ios_base::end);
    file_size_ = input_stream_.tellg();
    input_stream_.seekg(0, std::ios_base::beg);
  }
}

size_t PlainReader::GetSize() const {
  return file_size_;
}

std::istream *PlainReader::istream() {
  return &input_stream_;
}

CamelliaDecrypter::CamelliaDecrypter(const std::string &filename, const unsigned char key_string[16])
  : stream_buf_(key_string), input_stream_(&stream_buf_) {
  stream_buf_.open(filename.c_str());
  file_size_ = stream_buf_.size();
  input_stream_.rdbuf(&stream_buf_);
}

size_t CamelliaDecrypter::GetSize() const {
  return file_size_;
}

std::istream *CamelliaDecrypter::istream() {
  return &input_stream_;
}

SecondCryptoDecrypter::SecondCryptoDecrypter(const std::string &filename,
                                             const unsigned char key_string[16])
  : stream_buf_(key_string), input_stream_(&stream_buf_) {
  stream_buf_.open(filename.c_str());
  file_size_ = stream_buf_.size();
  input_stream_.rdbuf(&stream_buf_);
}

size_t SecondCryptoDecrypter::GetSize() const {
  return file_size_;
}

std::istream *SecondCryptoDecrypter::istream() {
  return &input_stream_;
}

} // namespace mlib
