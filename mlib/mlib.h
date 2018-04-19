#pragma once

/* mlib.h (updated on 2017/02/16)
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
/// @brief CipherType Enumerate
////////////////////////////////////////////////////////////////////////

enum CipherType : int {
  kCipherNone = 0,
  kCipherCamellia128 = 1,
  kCipherEqualMoreThanSLT = 2,
  kCipherOther = 3,
  kCipherFalse = -2
};

////////////////////////////////////////////////////////////////////////
/// @brief KeyInfo class
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
/// @brief Entry interface class
////////////////////////////////////////////////////////////////////////

class Entry {
public:
  /**
   * @brief A virtual destructor.
   */
  virtual ~Entry() = default;

  /**
   * @brief Check whether this entry is open.
   * @return true if this entry is open, and false otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual bool IsOpen() const noexcept = 0;

  /**
   * @brief Check whether this entry is a file.
   * @return true if this entry is an opened file, and false otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual bool IsFile() const noexcept = 0;

  /**
   * @brief Check whether this entry is a directory.
   * @return true if this entry is an opened directory, and false otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual bool IsDirectory() const noexcept = 0;

  /**
   * @brief Check whether this is a raw OS entry.
   * @return true if this is OSEntry, false otherwise (if MLibEntry).
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual bool IsRaw() const noexcept = 0;

  /**
   * @brief Returns this entry name.
   * @return this entry name.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual std::string GetName() const noexcept = 0;
  
  /**
   * @brief Returns the location of this entry.
   * @return the location of this entry.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual std::string GetLocation() const noexcept = 0;

  /**
   * @brief Returns the full path of this entry.
   * @return the full path of this entry.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual std::string GetFullPath() const noexcept = 0;

  /**
   * @brief Returns the file size of this entry.
   * @return the file size of this entry if a file, and 0 otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual size_t GetSize() const noexcept = 0;

  /**
   * @brief Seek the file position of this entry.
   * @return the file position this entry after seeking if a file, and -1 otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual off_t Seek(off_t offset, int whence) noexcept = 0;

  /**
   * @brief Read the file contents of this entry.
   * @return the read size of thie entry if a file, and 0 otherwise.
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual size_t Read(size_t size, void* dest) noexcept(false) = 0;
};

////////////////////////////////////////////////////////////////////////
/// @brief MLib class
////////////////////////////////////////////////////////////////////////

class MLib;
class Reader;

typedef std::shared_ptr<MLib> MLibPtr;

class MLib : public Entry {
protected:
  explicit MLib(const std::string& lib_name, Reader* reader);
  explicit MLib(MLib* parent);
  explicit MLib(const MLib&) = delete;
  MLib& operator=(const MLib&) = delete;
public:
  /**
   * @brief A factory method of a MLib entry.
   * @param[in] filename a MLib filename.
   * @param[in] product a product code name.
   * @return a shared pointer to a created MLib entry if success, and a null pointer otherwise.
   */ 
  static MLibPtr Open(const std::string &filename, const std::string &product);

  bool IsVerbose() const noexcept { return verbose_; }

  /**
   * @brief Check whether this entry is open.
   * @see Entry::IsOpen()
   */
  bool IsOpen() const noexcept override final {
    return reader_ != nullptr;
  }

  /**
   * @brief Check whether this entry is a directory.
   * @return true if this entry is an opened directory, and false otherwise.
   * @see Entry::IsDirectory()
   */
  bool IsDirectory() const noexcept override final {
    return IsOpen() && !IsFile();
  }

  /**
   * @brief Check whether this is a raw OS entry.
   * @return true if this is OSEntry, false otherwise (if MLibEntry).
   * @see Entry;;IsRaw()
   */
  bool IsRaw() const noexcept override final {
    return false;
  }

  /**
   * @brief A entry name getter.
   * @return a constant reference to this entry name.
   */
  virtual const std::string& name() const noexcept = 0;
  /**
   * @brief Returns this entry name.
   * @see Entry::GetName()
   */
  std::string GetName() const noexcept override final {
    return name();
  }

  /**
   * @brief A library filename getter.
   * @return the library filename of this entry.
   */
  const std::string& libname() const noexcept {
    return libname_;
  }

  /**
   * @brief A getter of the entry location in the library this entry belongs to.
   *        An entry location begins from '/', which means the root.
   * @return the entry location in the library this entry belongs to.
   */
  const std::string& location() const noexcept {
    return location_;
  }
  /**
   * @brief Returns the location of this entry.
   * @return the location of this entry.
   * @see Entry::GetLocation()
   */
  std::string GetLocation() const noexcept override final;

  /**
   * @brief Returns the full path of this entry.
   * @see Entry::GetFullPath()
   */
  std::string GetFullPath() const noexcept override final {
    return GetLocation() + name();
  }

  /**
   * @brief Returns the parent of this entry.
   * @return a pointer to the parent of this entry.
   */
  const MLibPtr& Parent() {
    return parent_;
  }

  /**
   * @brief Returns one of this child entries.
   * @param[in] i an index of this child entries (0 <= i < GetChildNumber())
   * @return a shared pointer to the ith child entry of this if exists,
   *         and a null pointer otherwise.
   */
  MLibPtr Child(size_t i) noexcept;
  /**
   * @brief Returns one of this child entries.
   * @param[in] name a child entry name
   * @return a shared pointer to the entry which has the given child name if exists,
   *         and a null pointer otherwise.
   */
  MLibPtr Child(const std::string& name) noexcept;
  /**
   * @brief A wrapper of Child(name)
   * @see MLib::Child()
   */
  MLibPtr operator[](const std::string &name) noexcept {
    return Child(name);
  }

  /**
   * @brief Returns the count of children that this entry has.
   * @return the count of children that this entry has
   * @note Any concrete derived class must override this pure virtual function.
   */
  virtual unsigned int GetChildNumber() const noexcept = 0;

  /**
   * @brief Returns the name of ith child entry of this.
   * @param[in] i an index of this child entries (0 <= i < GetChildNumber())
   * @return the name of ith child entry of this if exists,
   *         and an empty string otherwise.
   */
  std::string GetChildName(size_t i) const noexcept;

  /**
   * @brief Returns a vector list of this entry's child names.
   * @return a vector list of this entry's child names.
   */
  std::vector<std::string> GetChildNameList() const noexcept;

  /**
   * @brief Find an entry which has the given path.
   * @param[in] path a relative path from this entry.
   * @return a shared pointer to an entry which has the given path if found,
   *         a null pointer otherwise.
   */
  MLibPtr GetEntry(const std::string& path) noexcept;

  /**
   * @brief Get the children which this entry has.
   * @return a vector list of this entry's children.
   */
  std::vector<MLibPtr> GetChildren() noexcept;

  /**
   * @brief Check whether this entry has only directories (not has any file).
   * @return true if this entry has only directories, and false otherwise.
   */
  bool HasOnlyDirectories() const noexcept;

  /**
   * @brief Read this file contents.
   * @see Entry::Read()
   */
  size_t Read(size_t size, void *dest) noexcept(false) override final;
  /**
   * @brief Read this file contents from the given offset.
   * @see Read()
   */
  size_t Read(off_t offset, size_t size, void *dest);

  /**
   * @brief Returns the current file position of this entry.
   * @return the current file position of this entry.
   */
  off_t Tell() const noexcept;

  /**
   * @brief Seek the file position of this entry.
   * @see Entry::Seek()
   */
  off_t Seek(off_t new_pos, int whence) noexcept override;

  void List(const std::string &path) const;

  // utilities
  static const std::string FormatOldStylePath(const std::string &mlib,
                                              const std::string &internal_path);
  static const std::string FormatOldStylePath(const std::string &mlib,
                                              const std::string &location,
                                              const std::string &name);

protected:
  virtual void DoLoadChildInfo() noexcept = 0;
  virtual const char *GetChildNameAsCharArray(size_t i) const noexcept = 0;
  virtual MLib *CreateChild(size_t i) noexcept = 0;
  virtual off_t GetFileBaseOffset() const noexcept = 0;

private:
  const MLibPtr GetOrCreateChild(size_t i) noexcept;
  MLibPtr GetEntry(const std::string &path, size_t index) noexcept;
  void LoadChildInfo();

  bool verbose_;
  std::weak_ptr<MLib> self_;
  const MLibPtr parent_;
  std::string libname_;
  std::string location_;
  std::vector< std::weak_ptr<MLib> > children_;
  std::map<std::string, size_t> child_name2index_;
  std::shared_ptr<Reader> reader_;
  off_t file_pos_;

  static std::map< std::string, std::weak_ptr<MLib> > opened_libs_;
};

////////////////////////////////////////////////////////////////////////
/// @brief LIB_t class
////////////////////////////////////////////////////////////////////////

class LIB_t : public MLib {
public:
  explicit LIB_t(const std::string &lib_name, Reader *reader);
  ~LIB_t();

  /**
   * @see MLib::name()
   */
  const std::string &name() const noexcept override {
    return name_;
  }

  /**
   * @see Entry::IsFile()
   */
  bool IsFile() const noexcept override;

  /**
   * @see Entry::GetSize()
   */
  size_t GetSize() const noexcept override final {
    return static_cast<size_t>(file_size_);
  }

  /**
   * @see MLib::GetChildNumber()
   */
  unsigned int GetChildNumber() const noexcept override;

protected:
  void DoLoadChildInfo() noexcept override;
  const char *GetChildNameAsCharArray(size_t i) const noexcept override;
  MLib *CreateChild(size_t i) noexcept override;
  off_t GetFileBaseOffset() const noexcept override;

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
/// @brief LIBU_t class
////////////////////////////////////////////////////////////////////////

class LIBU_t : public MLib {
public:
  explicit LIBU_t(const std::string &lib_name, Reader *reader);
  ~LIBU_t();

  /**
   * @see MLib::name()
   */
  const std::string& name() const noexcept override {
    return name_;
  }

  /**
   * @see Entry::IsFile()
   */
  bool IsFile() const noexcept override;

  /**
   * @see Entry::GetSize()
   */
  size_t GetSize() const noexcept override final {
    return static_cast<size_t>(file_size_);
  }

  /**
   * @see MLib::GetChildNumber()
   */
  unsigned int GetChildNumber() const noexcept override;

protected:
  void DoLoadChildInfo() noexcept override;
  const char *GetChildNameAsCharArray(size_t i) const noexcept override;
  MLib *CreateChild(size_t i) noexcept override;
  off_t GetFileBaseOffset() const noexcept override;

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
/// @brief LIBP_t class
////////////////////////////////////////////////////////////////////////

class LIBP_t : public MLib {
public:
  explicit LIBP_t(const std::string &lib_name, Reader *reader, unsigned int data_alignment);
  ~LIBP_t();

  /**
   * @see MLib::name()
   */
  const std::string &name() const noexcept override {
    return name_;
  }

  /**
   * @see Entry::IsFile()
   */
  bool IsFile() const noexcept override;

  /**
   * @see Entry::GetSize()
   */
  size_t GetSize() const noexcept override final;

  /**
   * @see MLib::GetChildNumber()
   */
  unsigned int GetChildNumber() const noexcept override;

protected:
  void DoLoadChildInfo() noexcept override;
  const char *GetChildNameAsCharArray(size_t i) const noexcept override;
  MLib *CreateChild(size_t i) noexcept override;
  off_t GetFileBaseOffset() const noexcept override;

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
    explicit SharedObject(Reader *reader, unsigned int data_alignment);
  };

  LIBP_t(const std::shared_ptr<SharedObject> &shobj, LIBP_t *parent, unsigned int entry_index);

  std::shared_ptr<SharedObject> shobj_;
  const unsigned int entry_index_;
  const std::string name_;
};

////////////////////////////////////////////////////////////////////////
/// @brief OSEntry class
////////////////////////////////////////////////////////////////////////

class OSEntry : public Entry {
public:
  bool IsRaw() const noexcept override final {
    return true;
  }
};

////////////////////////////////////////////////////////////////////////
/// @brief OSFile class
////////////////////////////////////////////////////////////////////////

class OSFile : public OSEntry {
public:
  explicit OSFile(const std::string &name);
  explicit OSFile(const OSFile &f) = delete;
  explicit OSFile(OSFile &&f) noexcept;
  ~OSFile();
  bool IsOpen() const noexcept override;
  bool IsFile() const noexcept override;
  bool IsDirectory() const noexcept override {
    return false;
  }
  std::string GetName() const noexcept override;
  std::string GetLocation() const noexcept override;
  std::string GetFullPath() const noexcept override;
  size_t GetSize() const noexcept override;
  off_t Seek(off_t offset, int whence) noexcept override;
  size_t Read(size_t size, void *dest) noexcept(false) override;
private:
  FILE *fp_;
  std::string name_;
};

////////////////////////////////////////////////////////////////////////
/// @brief OSDirectory class
////////////////////////////////////////////////////////////////////////

class OSDirectory : public OSEntry {
public:
  explicit OSDirectory(const std::string &path);
  explicit OSDirectory(const OSDirectory &d) = delete;
  explicit OSDirectory(OSDirectory &&d) noexcept;
  ~OSDirectory();
  bool IsOpen() const noexcept override;
  bool IsFile() const noexcept override {
    return false;
  }
  bool IsDirectory() const noexcept override;
  std::string GetName() const noexcept override;
  std::string GetLocation() const noexcept override;
  std::string GetFullPath() const noexcept override;
  size_t GetSize() const noexcept override {
    return 0UL;
  }
  off_t Seek(off_t, int) noexcept override {
    return -1;
  }
  size_t Read(size_t, void*) noexcept(false) override {
    return 0UL;
  }
  OSFile* OpenFile(const std::string& filename) const noexcept;
  OSDirectory* OpenDirectory(const std::string& dirname) const noexcept;
  OSEntry* OpenChild(const std::string& child_name) const noexcept;
  std::vector<OSEntry*> GetChildren() const noexcept;
private:
  DIR *dirp_;
  std::string name_;
};

////////////////////////////////////////////////////////////////////////
/// @brief Versioned entry class
////////////////////////////////////////////////////////////////////////

class VersionedEntry : public Entry {
protected:
  VersionedEntry();
  VersionedEntry(OSEntry* p_os_entry, std::vector<MLibPtr>&& mlib_history) noexcept;
public:
  VersionedEntry(const std::string& path, const std::string& product);
  explicit VersionedEntry(const VersionedEntry&) = delete;
  explicit VersionedEntry(VersionedEntry&& e) noexcept;
  ~VersionedEntry();
  int GetLatestVersion() const noexcept;
  int GetCurrentVersion() const noexcept;
  void SwitchVersion(int new_version) noexcept;
  bool IsOpen() const noexcept override;
  bool IsFile() const noexcept override;
  bool IsDirectory() const noexcept override;
  bool IsRaw() const noexcept override;
  std::string GetName() const noexcept override;
  std::string GetLocation() const noexcept override;
  std::string GetFullPath() const noexcept override;
  size_t GetSize() const noexcept override;
  off_t Seek(off_t offset, int whence) noexcept override;
  size_t Read(size_t size, void* dest) noexcept(false) override;
  VersionedEntry* OpenChild(const std::string& child_name) const noexcept;
  std::vector<VersionedEntry*> GetChildren() const noexcept;
private:
  OSEntry* p_os_entry_;
  std::vector<MLibPtr> mlib_entries_;
  std::string name_;
  Entry* p_curr_;
  int curr_ver_;
};


////////////////////////////////////////////////////////////////////////
/// @brief mlib common functions
////////////////////////////////////////////////////////////////////////

size_t UTF16ToUTF8(const char *src, char *dst);
std::string UTF16ToUTF8(const char16_t *src, size_t length);
std::string UTF16ToUTF8(const std::u16string &src);
std::string GenerateFullPath(const std::string &path);
Reader *CreateReader(const std::string &filename, const std::string &product);
bool LoadKeyInfo(const std::string &csv);
bool FindKeyInfo(const std::string &product, const KeyInfo **dest);
void PrintKeyInfo();
unsigned int GetDataAlignment(const std::string &product);

} //namespace mlib
