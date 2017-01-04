/* exmaldat.cc (updated on 2016/12/29)
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
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include "mlib/reader.h"
#include "mlib/extractor.h"

// for test
#include <cstdlib>
#include <ctime>

bool create_test_data(const char *filename, size_t size) {
  std::ofstream ofs(filename);
  if (ofs.is_open() == false) {
    return false;
  }

  std::srand(std::time(0));

  char ch;
  std::locale loc;

  for (size_t i = 0; i < size; ++i) {
    if (i % 512 == 511) {
      ch = '\n';
    } else {
      do {
        ch = std::rand() & 0x7f;
        if (std::isalnum(ch, loc) || std::ispunct(ch, loc)) break;
      } while (true);
    }
    ofs.write(&ch, 1);
  }

  ofs.close();
  return true;
}

void print_usage() {
  std::cout << "usage: exmaldat <product-name> [-dfmwst] <input-file> [-p internal-path]\n"
            << "       [output-directory]\n\n"
            << "  d  : decrypt an archive, not extract (default: disable)\n"
            << "  f  : flatten directory structure (default: disable)\n"
            << "  m  : convert mgf into png (default: enable)\n"
            << "  w  : convert webp into png (default: enable on and after SGB)\n"
            << "  s  : skip svg files (default: disable)\n"
            << "  t  : concatenate textures (default: disable)\n"
            << "  t0 : as for -t, but extract level 0 textures only (default: enable)\n"
            << "  t1 : as for -t, but extract level 1 textures only (default: disable)\n"
            << "  t2 : as for -t, but extract level 2 textures only (default: disable)\n"
            << std::endl;
}

struct Parameters {
  std::string product_name;
  std::string lib_name;
  std::string internal_path;
  std::string output_directory;
  bool decrypt;
  bool flatten;
  bool mgf2png;
  bool webp2png;
  bool skip_svg;
  bool texcat;
  int tex_level;
  Parameters()
    : decrypt(false), flatten(false), mgf2png(true), webp2png(true),
      skip_svg(false), texcat(true), tex_level(0) {}
};

bool get_param(int argc, char **argv, Parameters *params) {
  if (params == NULL) return false;
  params->product_name.assign(argv[1]);
  for (int i = 2; i < argc; ++i) {
    std::string p = argv[i];
    std::string::iterator it = p.begin();
    const std::string::const_iterator it_end = p.end();
    if (p == "-p") {
      if (argc <= i + 1) {
        std::cerr << "ERROR: invalid parameter 'p'." << std::endl;
        return false;
      }
      ++i;
      params->internal_path.assign(argv[i]);
      continue;
    }
    if (*it == '-') {
      for (++it; it != it_end; ++it) {
        switch (*it) {
        case 'd':
          params->decrypt = true;
          break;
        case 'D':
          params->decrypt = false;
          break;
        case 'f':
          params->flatten = true;
          break;
        case 'm':
          params->mgf2png = true;
          break;
        case 'M':
          params->mgf2png = false;
          break;
        case 'w':
          params->webp2png = true;
          break;
        case 'W':
          params->webp2png = false;
          break;
        case 's':
          params->skip_svg = true;
          break;
        case 'S':
          params->skip_svg = false;
          break;
        case 't': {
            params->texcat = true;
            std::string::const_iterator it2 = it + 1;
            if (it2 == p.end()) {
              params->tex_level = -1;
              break;
            }
            switch (*it2) {
            case '0':
              ++it;
              params->tex_level = 0;
              break;
            case '1':
              ++it;
              params->tex_level = 1;
              break;
            case '2':
              ++it;
              params->tex_level = 2;
              break;
            }
          }
          break;
        case 'T':
          params->texcat = false;
          break;
        default:
          std::cerr << "ERROR: invalid parameter '" << *it << "'." << std::endl;
          return false;
        }
      }
    } else if (params->lib_name.empty()) {
      params->lib_name.assign(p);
    } else if (params->output_directory.empty()) {
      params->output_directory.assign(p);
    }
  }
  if (params->output_directory.empty()) {
    params->output_directory.assign(".");
  }
  return true;
}

bool decrypt(const std::string &product, const std::string& path) {
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
   return false;
  }
  mlib::LibReader *reader = new mlib::EncryptedLibReader(fd, product);
  if (reader == NULL) {
    ::close(fd);
    return false;
  }
  const size_t size = reader->GetSize();
  char *buf = new char[65536];
  std::ofstream ofs("decrypted.dat");
  for (size_t i = 0; i < size; i += 65536) {
    size_t read_size = 65536;
    if (size - i < 65536) {
      read_size = size - i;
    }
    reader->Read(i, read_size, buf);
    ofs.write(buf, read_size);
    if (size - i <= 65536) break;
  }
  ofs.close();
  delete [] buf;
  delete reader;
  ::close(fd);
  return true;
}

static mlib::Extractor extractor;

void signal_handler(int) {
  extractor.Stop();
}

int main(int argc, char **argv) {

  if (argc < 2) {
    print_usage();
    return 0;
  }

  std::string keyinfo_csv(argv[0]);
#ifdef _WINDOWS
  keyinfo_csv.erase(keyinfo_csv.find_last_of('\\') + 1);
#else
  keyinfo_csv.erase(keyinfo_csv.find_last_of('/') + 1);
#endif
  keyinfo_csv.append("key_info.csv");
  if (mlib::EncryptedLibReader::LoadKeyInfo(keyinfo_csv) == false) {
    return -1;
  }

  Parameters params;
  if (get_param(argc, argv, &params) == false) {
    return -1;
  }
  if (params.product_name == "--list-keys") {
    mlib::EncryptedLibReader::PrintKeyInfo();
    return 0;
  }

  if (params.decrypt == true) {
    std::cout << "-- Decrypting '" << params.lib_name << "'...";
    std::cout.flush();
    bool ret = decrypt(params.product_name, params.lib_name);
    if (ret == false) {
      std::cerr << "ERROR: failed to decrypt '" << params.lib_name << "'." << std::endl;
      return -1;
    }
    std::cout << "OK." << std::endl;
    return 0;
  }

  mlib::MLib *lib = mlib::MLib::Open(params.lib_name, params.product_name);
  if (lib == NULL) {
    return -1;
  }

  mlib::Extractor::Initialize();

  extractor.Flatten(params.flatten);
  extractor.EnableMGFToPNG(params.mgf2png);
  extractor.EnableWebPToPNG(params.webp2png);
  extractor.EnableSVG(!params.skip_svg);
  extractor.EnableTexCat(params.texcat);
  extractor.SetTexLevel(params.tex_level);

  ::signal(SIGINT, &signal_handler);
  extractor.Extract(lib, params.internal_path, params.output_directory);

  delete lib;

  mlib::Extractor::Finalize();

  return 0;
}
