/* extractor.cc (updated on 2017/02/16)
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
#include <unistd.h>
#include <cassert>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include "reader.h"
#include "extractor.h"

namespace {

bool recursive_mkdir(const std::string& dir) {
  struct stat st;
  if (::stat(dir.c_str(), &st) == 0) {
#if 0
    std::cout << "[Info] the directory '" << dir
              << "' already exists." << std::endl;
#endif
    return true;
  } else {
    auto delim_pos = dir.find_last_of(mlib::kPathDelim);
    if (delim_pos == std::string::npos) return false;
    auto parent_dir = dir.substr(0, delim_pos);
    bool result = recursive_mkdir(parent_dir);
    if (!result) return false;
    result = (::mkdir(dir.c_str(), 0755) == 0) ? true : false;
    if (result) {
      std::cout << "[Info] created a directory '" << dir << "'." << std::endl;
    } else {
      std::cerr << "[Error] failed to create a directory '"
                << dir << "'." << std::endl;
    }
    return result;
  }
}

/*const*/char mgf_header[] = "\x4d\x61\x6c\x69\x65\x47\x46\0";
const char png_header[] = "\x89PNG\x0d\x0a\x1a\x0a";

Sint64 sdl_custom_size(struct SDL_RWops *context) {
  using namespace mlib;
  VersionedEntry* p_entry = reinterpret_cast<VersionedEntry*>(context->hidden.unknown.data1);
  return static_cast<Sint64>(p_entry->GetSize());
}

Sint64 sdl_custom_seek(struct SDL_RWops *context, Sint64 offset, int whence) {
  using namespace mlib;
  VersionedEntry* p_entry = reinterpret_cast<VersionedEntry*>(context->hidden.unknown.data1);
  return p_entry->Seek(offset, whence);
}

size_t sdl_custom_read(struct SDL_RWops *context, void *ptr, size_t size, size_t maxnum) {
  using namespace mlib;
  VersionedEntry* p_entry = reinterpret_cast<VersionedEntry*>(context->hidden.unknown.data1);
  char *char_ptr = reinterpret_cast<char *>(ptr);
  size_t read_size = size * maxnum;
  size_t ret = 0;
  off_t pos = p_entry->Seek(0, SEEK_CUR); /*p_entry->Tell();*/
  if (pos < 8 && reinterpret_cast<char *>(context->hidden.unknown.data2) == mgf_header) {
    const size_t header_read_size = std::min(static_cast<unsigned long long>(8), pos + read_size) - pos;
    ::memcpy(char_ptr, png_header + pos, header_read_size);
    char_ptr += header_read_size;
    read_size -= header_read_size;
    p_entry->Seek(header_read_size, SEEK_CUR);
    ret = header_read_size;
  }
  ret += p_entry->Read(read_size, char_ptr);
  return ret;
}

size_t sdl_custom_write(struct SDL_RWops* /*context*/, const void* /*ptr*/, size_t /*size*/, size_t /*num*/) {
  return 0;
}

int sdl_custom_close(struct SDL_RWops* /*context*/) {
  return 0;
}

SDL_RWops *SDL_RWFromMLib(mlib::VersionedEntry* p_entry) {
  SDL_RWops *rwops;
  rwops = SDL_AllocRW();
  if (rwops) {
    rwops->size = &sdl_custom_size;
    rwops->seek = &sdl_custom_seek;
    rwops->read = &sdl_custom_read;
    rwops->write = &sdl_custom_write;
    rwops->close = &sdl_custom_close;
    rwops->type = SDL_RWOPS_UNKNOWN;
    rwops->hidden.unknown.data1 = p_entry;
    if (8 <= p_entry->GetSize()) {
      char buf[8];
      p_entry->Seek(0, SEEK_SET);
      p_entry->Read(8, buf);
      if (::memcmp(buf, mgf_header, 8) == 0) {
        rwops->hidden.unknown.data2 = static_cast<void *>(mgf_header);
      } else {
        rwops->hidden.unknown.data2 = nullptr;
      }
    }
    p_entry->Seek(0, SEEK_SET);
  }
  return rwops;
}

} // namespace

namespace mlib {

void Extractor::Initialize() {
  SDL_Init(SDL_INIT_VIDEO);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_WEBP);
}

void Extractor::Finalize() {
  SDL_Quit();
  IMG_Quit();
}

bool Extractor::TexCat(VersionedEntry* p_dzi, const VersionedEntry* p_tex_entry, const std::string &fs_path, std::vector<char> &buf) {
  assert(p_dzi != nullptr && p_dzi->IsFile() &&
         p_tex_entry != nullptr && p_tex_entry->IsDirectory());
  const unsigned int dzi_size = p_dzi->GetSize();
  std::vector<char> dzi_buf(dzi_size + 1, 0);
  char* dzi_ptr = &dzi_buf[0];
  off_t file_pos_tmp = p_dzi->Seek(0, SEEK_CUR); /*p_dzi->Tell();*/
  p_dzi->Seek(0, SEEK_SET);
  p_dzi->Read(dzi_size, dzi_ptr);
  p_dzi->Seek(file_pos_tmp, SEEK_SET);
  dzi_ptr[dzi_size] = '\0';

  if (dzi_ptr[0] != 'D' || dzi_ptr[1] != 'Z' || dzi_ptr[2] != 'I') {
    return Extract(p_dzi, fs_path, buf);
  }

  const std::string out_name_base(p_dzi->GetName().substr(0, p_dzi->GetName().length() - 4));

  dzi_ptr += 3;
  while (::isspace(*dzi_ptr)) { ++dzi_ptr; }

  std::cout << "-- Extracting '" << p_dzi->GetFullPath()
            << "' as PNG file...";
  std::cout.flush();

  try {
    std::istringstream ss(dzi_ptr);
    std::string token;
    std::getline(ss, token, ',');
    int width = std::stoi(token);
    std::getline(ss, token);
    int height = std::stoi(token);
    // std::cerr << "DEBUG: DZI (width, height) = (" << width << ", " << height << ')' << std::endl;
    std::getline(ss, token);
    const int lv_max = std::stoi(token);
    // std::cerr << "DEBUG: DZI levels = " << lv_max << std::endl;

    for (int l = 0; l < lv_max; ++l, width >>= 1, height >>= 1) {
      std::getline(ss, token, ',');
      const int cols = std::stoi(token);
      std::getline(ss, token);
      const int rows = std::stoi(token);
      // std::cerr << "DEBUG: DZI level " << l << " (cols, rows) = (" << cols << ", " << rows << ')' << std::endl;

      if (0 <= texlv_ && l != texlv_) {
        for (int i = 0; i < rows; ++i) {
          std::getline(ss, token);
        }
        continue;
      }

      SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32,
                                                  0x0000ff00, 0x00ff0000, 0xff000000, 0x000000ff);
      bool warning_caused = false;
      for (int i = 0; i < 256 * rows; i += 256) {
        const int tex_height = std::min(256, height - i);
        std::getline(ss, token);
        std::istringstream ss_col(token);
        for (int j = 0; j < 256 * cols; j += 256) {
          const int tex_width = std::min(256, width - j);
          SDL_Rect rect = { j, i, tex_width, tex_height };
          std::string tex_name;
          std::getline(ss_col, tex_name, ',');
          if (tex_name.back() == '\n') { tex_name.pop_back(); }
          if (tex_name.back() == '\r') { tex_name.pop_back(); }
  #ifndef _WINDOWS
          std::replace(tex_name.begin(), tex_name.end(), '\\', '/');
  #endif
          // std::cerr << "DEBUG: col_name = " << tex_name << std::endl;
          if (tex_name.empty()) continue;
          VersionedEntry* p_tex_file = p_tex_entry->OpenChild(tex_name + ".mgf");
          if (p_tex_file == nullptr || !p_tex_file->IsFile()) {
            p_tex_file = p_tex_entry->OpenChild(tex_name + ".png");
            if (p_tex_file == nullptr || !p_tex_file->IsFile()) {
              p_tex_file = p_tex_entry->OpenChild(tex_name + ".webp");
              if (p_tex_file == nullptr || !p_tex_file->IsFile()) {
                if (warning_caused == false) {
                  warning_caused = true;
                  std::cout << std::endl;
                }
                std::cerr << " -- Warning: failed to open a tex-file '"
                          << tex_name << "'." << std::endl;
                continue;
              }
            }
          }
          // std::cout << " -- Loading " << p_tex_file->GetName() << "...";
          // std::cout.flush();
          SDL_RWops *rwops = SDL_RWFromMLib(p_tex_file);
          SDL_Surface *tex_surface = IMG_Load_RW(rwops, 0);
          SDL_BlitSurface(tex_surface, nullptr, surface, &rect);
          SDL_FreeSurface(tex_surface);
          SDL_RWclose(rwops);
          // std::cout << "OK." << std::endl;
        }
      }

      std::string out_name(out_name_base);
      if (texlv_ < 0) {
        out_name.append(1, '_');
        out_name.append(std::to_string(l));
      }
      out_name.append(".png");
      // std::cout << " -- Saving as '" << out_name << "'...";
      std::string out_fullname(fs_path);
      out_fullname.append(1, kPathDelim);
      out_fullname.append(out_name);
      IMG_SavePNG(surface, out_fullname.c_str());
      SDL_FreeSurface(surface);
      if (warning_caused == false) {
        std::cout << "OK." << std::endl;
      }
      if (l == texlv_) break;
    }
  } catch (std::invalid_argument& e) {
    std::cout << "Skipped because this file is wrong." << std::endl;
    return false;
  }
  return true;
}

bool Extractor::Extract(VersionedEntry* p_entry, const std::string& fs_path, std::vector<char> &buf) {
  if (p_entry == nullptr || !p_entry->IsOpen()) return false;

  const std::string entry_name = p_entry->GetName();
  std::string fs_path_tmp = fs_path;

  if (p_entry->IsFile()) {
    if (stop_) return true;
    if (p_entry->IsRaw()) {
      std::cout << "-- Skip extracting '" << p_entry->GetFullPath()
                << "' because of a raw entry." << std::endl;
      return true;
    }
    unsigned int size = p_entry->GetSize();
    std::cout << "-- Extracting '" << p_entry->GetFullPath() << "...";
    std::cout.flush();

    off_t file_pos_tmp = p_entry->Seek(0, SEEK_CUR); /*p_entry->Tell()*/;
    p_entry->Seek(0, SEEK_SET);
    if (buf.size() < size) {
      buf.assign(size, 0);
    }
    char *const buf_ptr = &buf[0];
    size = p_entry->Read(size, buf_ptr);
    p_entry->Seek(file_pos_tmp, SEEK_SET);

    size_t entry_ext_index = entry_name.find_last_of(".");
    const std::string entry_ext =
        (entry_ext_index == std::string::npos) ? "" :
        entry_name.substr(entry_name.find_last_of("."));

    fs_path_tmp.append(entry_name);

    if (mgf2png_ && entry_ext == ".mgf") {
      const bool is_mgf = size >= 8 && !::memcmp(buf_ptr, mgf_header, 8);
      if (is_mgf) {
        fs_path_tmp = fs_path_tmp.substr(0, fs_path_tmp.size() - 4);
        fs_path_tmp.append(".png");
        ::memcpy(buf_ptr, png_header, 8);
      }
    }

    if (webp2png_ && entry_ext == ".webp") {
      fs_path_tmp.erase(fs_path_tmp.size() - 5);
      fs_path_tmp.append(".png");
      SDL_RWops *ops = SDL_RWFromConstMem(&buf[0], size);
      SDL_Surface *surface = IMG_Load_RW(ops, 0);
      IMG_SavePNG(surface, fs_path_tmp.c_str());
      SDL_FreeSurface(surface);
      SDL_RWclose(ops);
    } else {
      std::ofstream ofs(fs_path_tmp.c_str(), std::ios::out | std::ios::binary);
      if (ofs.is_open() == false) {
        std::cerr << "failed to create the file '" << fs_path_tmp << "'.";
        std::cout << std::endl;
        return false;
      }
      ofs.write(buf_ptr, size);
      ofs.close();
    }

    std::cout << "OK." << std::endl;
    return true;
  }

  if (flatten_ == false) {
    fs_path_tmp.append(entry_name);
    if (false == recursive_mkdir(fs_path_tmp)) {
      return false;
    }
    if ( !entry_name.empty() ) {
      fs_path_tmp.push_back(kPathDelim);
    }
  }

  VersionedEntry* p_tex_entry = nullptr;
  if (texcat_) {
    p_tex_entry = p_entry->OpenChild("tex");
    if (p_tex_entry && !p_tex_entry->IsDirectory()) {
      delete p_tex_entry;
      p_tex_entry = nullptr;
    }
  }
#if 1
  std::cout << "[Info] Extractor: start searching the children of '"
            << p_entry->GetFullPath() << '.' << std::endl;
#endif
  std::vector<VersionedEntry*> children = p_entry->GetChildren();
  for (auto& p_child : children) {
    if (stop_) break;
    const std::string child_name = p_child->GetName();
    size_t child_name_ext_index = child_name.find_last_of(".");
    const std::string child_ext =
        (child_name_ext_index == std::string::npos) ? "" :
        child_name.substr(child_name.find_last_of("."));
    if ((texcat_ && child_name == "tex") ||
        (svg_ == false && child_ext == ".svg")) {
      std::cout << "-- Skip '" << p_entry->GetFullPath()
                << "'." << std::endl;
      continue;
    }
    if (p_tex_entry && child_ext == ".dzi") {
      TexCat(p_child, p_tex_entry, fs_path_tmp, buf);
    } else {
      Extract(p_child, fs_path_tmp, buf);
    }
  }
#if 1
  std::cout << "[Info] Extractor: end searching the children of '"
            << p_entry->GetFullPath() << '.' << std::endl;
#endif
  if (p_tex_entry) {
    delete p_tex_entry;
  }
  while ( !children.empty() ) {
    VersionedEntry* p_child = children.back();
    delete p_child;
    children.pop_back();
  }
  return true;
}

bool Extractor::Extract(VersionedEntry* p_entry, const std::string& /*fs_path*/) {
  stop_ = false;

#if 0
  std::string fs_path_tmp = fs_path;
  std::replace(fs_path_tmp.begin(), fs_path_tmp.end(), kPathDelimNotUsed, kPathDelim);
  if (*fs_path_tmp.crbegin() != kPathDelim) {
    fs_path_tmp.push_back(kPathDelim);
  }
#else
  std::cerr << "[Warning] Extractor: the given output path will be ignored." << std::endl;
  if ( !p_entry->IsDirectory() ) {
    std::cerr << "[Error] Extractor: failed to open '" << p_entry->GetFullPath()
              << "' as a directory." << std::endl;
    return false;
  }
  std::string fs_path_tmp = p_entry->GetLocation();
#endif
  std::vector<char> buf;  // for reading file contents
  const clock_t clk = ::clock();
  bool ret = Extract(p_entry, fs_path_tmp, buf);
  if (ret == false) {
    std::cerr << "[Error] Extractor: failed to extract files." << std::endl;
    return ret;
  }
  double elapsed = static_cast<double>(::clock() - clk) / CLOCKS_PER_SEC;
  int elapsed_sec = static_cast<int>(::round(elapsed));
  int elapsed_min = elapsed_sec / 60;
  elapsed_sec %= 60;
  std::cout << "Elapsed time: " << elapsed_min << " min "
            << elapsed_sec << " sec. (" << elapsed << " seconds)" << std::endl;
  return ret;
}

} // namespace mlib
