#pragma once

/* mlib.h (updated on 2017/02/16)
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
#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

namespace mlib {

////////////////////////////////////////////////////////////////////////
/// \brief KeyInfo class
////////////////////////////////////////////////////////////////////////

class KeyInfo {
public:
  KeyInfo() : key_string_{0}, cipher_type_(0) {}
  ~KeyInfo() = default;
  const unsigned char *key_string() const { return key_string_; }
  int cipher_type() const { return cipher_type_; }
  bool IsAsciiKey() const {
    for (auto i = 0; i < 16; ++i) {
      const auto &c = key_string_[i];
      // check if the key string match '[!-+\--~]'
      if ( ('!' <= c && c <= '+') || ('-' <= c && c <= '~') ) continue;
      else return false;
    }
    return true;
  }
private:
  friend bool LoadKeyInfo(const std::string &csv);
  unsigned char key_string_[16];
  int cipher_type_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The MLib class
////////////////////////////////////////////////////////////////////////

class LibReader;

class MLib {
public:
  explicit MLib(LibReader *reader);
  virtual ~MLib();

  static MLib *Open(const std::string &filename, const std::string &product);

  bool IsVerbose() const { return verbose_; }

  virtual const std::string &GetName() const = 0;
  const std::string &GetLocation() const;
  virtual bool IsFile() const = 0;
  bool IsDirectory() const ;
  virtual unsigned int GetFileSize() const = 0;

  MLib *Parent();
  MLib *Child(size_t i);
  MLib *Child(const std::string &name);
  virtual unsigned int GetChildNumber() const = 0;

  MLib *GetEntry(const std::string &path);

  size_t Read(size_t size, void *dest);
  off_t Tell() const;
  off_t Seek(off_t new_pos, int whence);

  void List(const std::string &path) const;

protected:
  explicit MLib(MLib *parent);
  void AppendChild(MLib *child);

  virtual void DoLoadChild() = 0;
  virtual off_t GetFileBaseOffset() const = 0;

private:
  MLib *GetEntry(const std::string &path, size_t index);
  void LoadChild();

  bool verbose_;
  MLib *const parent_;
  std::string location_;
  std::vector<MLib *> children_;
  boost::shared_ptr<LibReader> reader_;
  off_t file_pos_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The LIB_t class
////////////////////////////////////////////////////////////////////////

class LIB_t : public MLib {
public:
  LIB_t(LibReader *reader);

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;
  unsigned int GetChildNumber() const;

protected:
  void DoLoadChild();
  off_t GetFileBaseOffset() const;

private:
  struct LIBHDR {
    char signature[4];
    unsigned int unk1;
    unsigned int entry_count;
    unsigned int unk2;
  };

  struct LIBENTRY {
    char file_name[36];
    unsigned int length;
    unsigned int offset;
    unsigned int unknown;
  };

  LIB_t(LIB_t *parent, const LIBENTRY &entry_info);

  LIBHDR hdr_;
  std::vector<LIBENTRY> entries_;
  std::string name_;
  off_t offset_;
  size_t file_size_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The LIBU_t class
////////////////////////////////////////////////////////////////////////

class LIBU_t : public MLib {
public:
  LIBU_t(LibReader *reader);

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;
  unsigned int GetChildNumber() const;

protected:
  void DoLoadChild();
  off_t GetFileBaseOffset() const;

private:
  struct LIBUHDR {
    char signature[4];
    unsigned int unk1;
    unsigned int entry_count;
    unsigned int unk2;
  };

  struct LIBUENTRY {
    char file_name[66];
    unsigned int length;
    unsigned int offset;
    unsigned int unknown;
  };

  LIBU_t(LIBU_t *parent, const LIBUENTRY &entry_info);

  LIBUHDR hdr_;
  std::vector<LIBUENTRY> entries_;
  std::string name_;
  off_t offset_;
  size_t file_size_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The LIBP_t class
////////////////////////////////////////////////////////////////////////

class LIBP_t : public MLib {
public:
  LIBP_t(LibReader *reader);

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;

  unsigned int GetChildNumber() const;

protected:
  void DoLoadChild();
  off_t GetFileBaseOffset() const;

private:
  struct LIBPHDR {
    char signature[4];
    unsigned int entry_count;
    unsigned int file_count;
    unsigned int unknown;
  };

  struct LIBPENTRY {
    char file_name[20];
    unsigned int flags;
    unsigned int offset_index;
    unsigned int length;
    static const int kFlagFile = 0x10000;
  };

  struct SharedObject {
    LIBPHDR hdr_;
    std::vector<LIBPENTRY> entries_;
    std::vector<unsigned int> file_offsets_;
    size_t data_base_offset_;
  public:
    SharedObject(LibReader *reader);
  };

  LIBP_t(const boost::shared_ptr<SharedObject> &shobj, LIBP_t *parent, unsigned int entry_index);

  boost::shared_ptr<SharedObject> shobj_;
  const unsigned int entry_index_;
  const std::string name_;
};

////////////////////////////////////////////////////////////////////////
/// \brief mlib common functions
////////////////////////////////////////////////////////////////////////

bool LoadKeyInfo(const std::string &csv);
bool FindKeyInfo(const std::string &product, const KeyInfo **dest);
void PrintKeyInfo();

} //namespace mlib
