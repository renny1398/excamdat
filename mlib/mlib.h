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

#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <memory>
// #include <std/shared_ptr.hpp>
// #include <std/weak_ptr.hpp>

namespace mlib {

#ifdef _WINDOWS
const char kPathDelim = '\\';
const char kPathDelimNotUsed = '/';
#else
const char kPathDelim = '/';
const char kPathDelimNotUsed = '\\';
#endif

////////////////////////////////////////////////////////////////////////
/// \brief CipherType Enumerate
////////////////////////////////////////////////////////////////////////

enum CipherType : int {
  kCipherNone = 0,
  kCipherCamellia128 = 1,
  kCipherEqualMoreThanSLT = 2,
  kCipherOther = 3,
  kCipherFalse = -2
};

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
  unsigned int data_alignment() const { return data_alignment_; }
private:
  friend bool LoadKeyInfo(const std::string &csv);
  unsigned char key_string_[16];
  int cipher_type_;
  unsigned int data_alignment_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The MLib class
////////////////////////////////////////////////////////////////////////

class MLib;
class LibReader;

typedef std::shared_ptr<MLib> MLibPtr;

class MLib {
public:
  explicit MLib(const std::string &lib_name, LibReader *reader);
  virtual ~MLib();

  static MLibPtr Open(const std::string &filename, const std::string &product);

  bool IsVerbose() const { return verbose_; }

  virtual const std::string &GetName() const = 0;
  const std::string &GetLibraryName() const;
  const std::string &GetLocation() const;
  virtual bool IsFile() const = 0;
  bool IsDirectory() const ;
  virtual unsigned int GetFileSize() const = 0;

  const MLibPtr &Parent();
  MLibPtr Child(size_t i);
  MLibPtr Child(const std::string &name);
  virtual unsigned int GetChildNumber() const = 0;
  const std::string GetChildName(size_t i) const;

  MLibPtr GetEntry(const std::string &path);
  bool GetAllChildren(std::vector<MLibPtr> *dest);

  size_t Read(size_t size, void *dest);
  off_t Tell() const;
  off_t Seek(off_t new_pos, int whence);

  void List(const std::string &path) const;

  // utilities
  static const std::string FormatOldStylePath(const std::string &mlib,
                                              const std::string &internal_path);
  static const std::string FormatOldStylePath(const std::string &mlib,
                                              const std::string &location,
                                              const std::string &name);

protected:
  explicit MLib(MLib *parent);

  const MLibPtr CreateChild(size_t i);

  virtual void DoLoadChildInfo() = 0;
  virtual const char *GetChildNameAsCharArray(size_t i) const = 0;
  virtual MLib *DoCreateChild(size_t i) = 0;
  virtual off_t GetFileBaseOffset() const = 0;

private:
  MLibPtr GetEntry(const std::string &path, size_t index);
  void LoadChildInfo();

  bool verbose_;
  std::weak_ptr<MLib> self_;
  const MLibPtr parent_;
  std::string libname_;
  std::string location_;
  std::vector< std::weak_ptr<MLib> > children_;
  std::map<std::string, size_t> child_name2index_;
  std::shared_ptr<LibReader> reader_;
  off_t file_pos_;

  static std::map< std::string, std::weak_ptr<MLib> > opened_libs_;
};

////////////////////////////////////////////////////////////////////////
/// \brief The LIB_t class
////////////////////////////////////////////////////////////////////////

class LIB_t : public MLib {
public:
  explicit LIB_t(const std::string &lib_name, LibReader *reader);
  ~LIB_t();

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;
  unsigned int GetChildNumber() const;

protected:
  void DoLoadChildInfo();
  const char *GetChildNameAsCharArray(size_t i) const;
  MLib *DoCreateChild(size_t i);
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
  explicit LIBU_t(const std::string &lib_name, LibReader *reader);
  ~LIBU_t();

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;
  unsigned int GetChildNumber() const;

protected:
  void DoLoadChildInfo();
  const char *GetChildNameAsCharArray(size_t i) const;
  MLib *DoCreateChild(size_t i);
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

  LIBU_t(LIBU_t* parent, const LIBUENTRY &entry_info);

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
  explicit LIBP_t(const std::string &lib_name, LibReader *reader, unsigned int data_alignment);
  ~LIBP_t();

  const std::string &GetName() const;
  bool IsFile() const;
  unsigned int GetFileSize() const;

  unsigned int GetChildNumber() const;

protected:
  void DoLoadChildInfo();
  const char *GetChildNameAsCharArray(size_t i) const;
  MLib *DoCreateChild(size_t i);
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
    static const int kFlagFile2 = 0x20000;
  };

  struct SharedObject {
    LIBPHDR hdr_;
    std::vector<LIBPENTRY> entries_;
    std::vector<unsigned int> file_offsets_;
    size_t data_base_offset_;
  public:
    explicit SharedObject(LibReader *reader, unsigned int data_alignment);
  };

  LIBP_t(const std::shared_ptr<SharedObject> &shobj, LIBP_t *parent, unsigned int entry_index);

  std::shared_ptr<SharedObject> shobj_;
  const unsigned int entry_index_;
  const std::string name_;
};

////////////////////////////////////////////////////////////////////////
/// \brief File classes
////////////////////////////////////////////////////////////////////////

class File {
public:
  virtual ~File() = default;
  const File &operator=(const File &rhs) = delete;
  virtual bool IsOpened() const = 0;
  virtual const std::string GetFullPath() const = 0;
  virtual size_t GetSize() const = 0;
  virtual off_t Seek(off_t offset, int whence) = 0;
  virtual size_t Read(size_t size, void *dest) = 0;
};

class OSFile : public File {
public:
  explicit OSFile(const std::string &name);
  explicit OSFile(const OSFile &f) = delete;
  explicit OSFile(OSFile &&f) noexcept;
  ~OSFile();
  bool IsOpened() const;
  const std::string GetFullPath() const;
  size_t GetSize() const;
  off_t Seek(off_t offset, int whence);
  size_t Read(size_t size, void *dest);
private:
  FILE *fp_;
  std::string name_;
};

class MLibEmbbedFile : public File {
public:
  MLibEmbbedFile(const std::string &name, const std::string &product);
  explicit MLibEmbbedFile(const MLibPtr &mlib);
  explicit MLibEmbbedFile(const MLibEmbbedFile &f) = delete;
  explicit MLibEmbbedFile(MLibEmbbedFile &&f) noexcept;
  ~MLibEmbbedFile();
  bool IsOpened() const;
  const std::string GetFullPath() const;
  size_t GetSize() const;
  off_t Seek(off_t offset, int whence);
  size_t Read(size_t size, void *dest);
private:
  MLibPtr p_file_;
};

class VersionedFile {
public:
  VersionedFile(const std::string &name, const std::string &product);
  explicit VersionedFile(const VersionedFile &f) = delete;
  explicit VersionedFile(std::vector<File *> &&f) noexcept;
  ~VersionedFile();
  bool IsOpened() const;
  int GetVersion() const;
  int GetPointingAt() const;
  void PointAt(int point_at);
  const std::string GetFullPath() const;
  size_t GetSize() const;
  off_t Seek(off_t offset, int whence);
  size_t Read(size_t size, void *dest);
private:
  std::vector<File *> p_files_;
  File *p_;
  int point_at_;
};

////////////////////////////////////////////////////////////////////////
/// \brief Directory classes
////////////////////////////////////////////////////////////////////////

class Directory {
public:
  virtual ~Directory() = default;
  const Directory &operator=(const Directory &rhs) = delete;
  virtual bool IsOpened() const = 0;
  virtual const std::string GetFullPath() const = 0;
  virtual File *OpenFile(const std::string &filename) const = 0;
};

class OSDirectory : public Directory {
public:
  explicit OSDirectory(const std::string &path);
  explicit OSDirectory(const OSDirectory &d) = delete;
  explicit OSDirectory(OSDirectory &&d) noexcept;
  ~OSDirectory();
  bool IsOpened() const;
  const std::string GetFullPath() const;
  File *OpenFile(const std::string &filename) const;
private:
  DIR *dirp_;
  std::string name_;
};

class MLibDirectory : public Directory {
public:
  MLibDirectory(const std::string &path, const std::string &product);
  explicit MLibDirectory(const MLibPtr &mlib);
  explicit MLibDirectory(const MLibDirectory &d) = delete;
  explicit MLibDirectory(MLibDirectory &&d) noexcept;
  ~MLibDirectory();
  bool IsOpened() const;
  const std::string GetFullPath() const;
  File *OpenFile(const std::string &filename) const;
private:
  MLibPtr p_dir_;
};

class VersionedDirectory {
public:
  VersionedDirectory(const std::string &path, const std::string &product);
  explicit VersionedDirectory(const VersionedDirectory &d) = delete;
  ~VersionedDirectory();
  bool IsOpened() const;
  int GetVersion() const;
  int GetPointingAt() const;
  void PointAt(int point_at);
  const std::string GetFullPath() const;
  VersionedFile *OpenFile(const std::string &filename) const;
private:
  std::vector<Directory *> p_dirs_;
  Directory *p_;
  int point_at_;
};

////////////////////////////////////////////////////////////////////////
/// \brief mlib common functions
////////////////////////////////////////////////////////////////////////

std::string GenerateFullPath(const std::string &path);
LibReader *CreateReader(const std::string &filename, const std::string& product);
bool LoadKeyInfo(const std::string &csv);
bool FindKeyInfo(const std::string &product, const KeyInfo **dest);
void PrintKeyInfo();

} //namespace mlib
