#pragma once

/* lib.h (updated on 2015/09/26)
 * Copyright (C) 2015 renny1398.
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

#include <fstream>
#include "camellia.h"


class Lib {

public:
  Lib(const std::string& file_name);
  virtual ~Lib() {}

  virtual void Extract(bool flatten = false) = 0;

  static Lib* Open(const std::string& file_name);

  bool IsVerbose() const { return is_verbose_; }
  void Verbose(bool verbose = true) { is_verbose_ = verbose; }

protected:
  static void DecryptBlock(std::ifstream& ifs, const KEY_TABLE_TYPE keyTable, void* buff, unsigned int len, unsigned int offset);

  std::ifstream ifs_;

private:
  bool is_verbose_;
};



class EncryptedLib : public Lib {

public:
  EncryptedLib(const std::string& file_name, const KEY_TABLE_TYPE keyTable);
  void Read(void* buff, unsigned int len, unsigned int offset);

private:
  KEY_TABLE_TYPE key_table_;
};




struct LIBUHDR {
  char signature[4];
  unsigned int unk1;
  unsigned int entry_count;
  unsigned int unk2;
};


struct LIBUENTRY {
  wchar_t file_name[32 + 1];
  unsigned int length;
  unsigned int offset;
  unsigned int unknown1;
};




struct LIBPHDR {
  char signature[4];
  unsigned int entry1_count;
  unsigned int entry2_count;
  unsigned int unknown1;
};



struct LIBPENTRY1 {
  char file_name[20];
  unsigned int flags;
  unsigned int offset_index;
  unsigned int length;
  static const int kFlagFile = 0x10000;
};



struct LIBPENTRY2 {
  unsigned int offset;
};



class Libu_t : public EncryptedLib {

public:
  Libu_t(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBUHDR& header);

  void Extract(bool flatten);

  // static Libu_t* Create(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBUHDR& header);

protected:
  void Extract(bool flatten, const std::string& prefix, unsigned int offset, unsigned int length);
};


class Libp_t : public EncryptedLib {

public:
  Libp_t(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBPHDR& header);
  virtual ~Libp_t();

  void Extract(bool flatten);

  // static Libp_t* Create(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBPHDR& header);

protected:
  void Extract(bool flatten, const std::string& prefix, unsigned int entry1_offset, unsigned int entry_count);

private:
  unsigned int base_offset_;
  unsigned int entry1_count_;
  unsigned int entry2_count_;
  LIBPENTRY1* entries1_;
  LIBPENTRY2* entries2_;
};
