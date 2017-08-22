/* create_drama_voice.cc (updated on 2017/08/31)
 * Copyright (C) 2017 renny1398.
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

#include "mlib/mlib.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>

int main(int argc, char **argv) {

  if (argc < 4) {
    std::cout << "Usage: create_drama_voice <product_name> <scenario.txt> <voice_path>" << std::endl;
    return 0;
  }

  std::string keyinfo_csv(argv[0]);
  keyinfo_csv.erase(keyinfo_csv.find_last_of(mlib::kPathDelim) + 1);
  keyinfo_csv.append("..");
  keyinfo_csv.append(1, mlib::kPathDelim);
  keyinfo_csv.append("key_info.csv");
  if (mlib::LoadKeyInfo(keyinfo_csv) == false) {
    std::cerr << "ERROR: failed to open the key_info file '" << keyinfo_csv << "'." << std::endl;
    return -1;
  }

  std::string product_name = argv[1];
  std::string base_entry_path = argv[3];
  if (base_entry_path.back() == mlib::kPathDelim) {
    base_entry_path.pop_back();
  }
  auto *base_entry = new mlib::VersionedDirectory(base_entry_path, product_name);
  if (base_entry->IsOpened() == false) {
    std::cerr << "ERROR: failed to open a voice directory." << std::endl;
    delete base_entry;
    return -1;
  }
  std::cout << "Voice version is " << base_entry->GetVersion() << '.' << std::endl;

  const std::string text_name(argv[2]);
  std::ifstream exec7(text_name);
  if (exec7.is_open() == false) {
    std::cerr << "ERROR: failed to open " << argv[2] << '.' << std::endl;
    delete base_entry;
    return -1;
  }

  std::string dir_name(product_name);
  dir_name.append(".drama");
  ::mkdir(dir_name.c_str(), 0755);

  if (text_name != "scenario.txt" && text_name != "exec7.txt") {
    std::string subdir_name(text_name);
    auto dot_index = subdir_name.find(".");
    if (dot_index != std::string::npos) {
      dir_name.append(1, mlib::kPathDelim);
      dir_name.append(subdir_name.substr(0, dot_index));
      ::mkdir(dir_name.c_str(), 0755);
    }
  }

  std::string lower_product_name;
  std::transform(product_name.begin(), product_name.end(), lower_product_name.begin(), ::tolower);
  int seq = 0;
  for (std::string l; std::getline(exec7, l); ) {
    for (size_t pos = 0; (pos = l.find("v_", pos)) != std::string::npos; ) {
      pos += 2;
      std::string chara_name;
      for (; pos < l.length() && std::isalpha(l.at(pos)); ++pos) {
        chara_name.push_back(l[pos]);
      }
      std::string voice_number;
      for (; pos < l.length() && std::isdigit(l.at(pos)); ++pos) {
        voice_number.push_back(l[pos]);
      }
      for (; pos < l.length() && (l.at(pos) == '_' || std::isalpha(l.at(pos))); ++pos) {
        voice_number.push_back(l[pos]);
      }
      std::string voice_name("v_");
      voice_name.append(chara_name);
      voice_name.append(voice_number);
      voice_name.append(".ogg");
      std::string voice_filename;
      voice_filename.assign(chara_name);
      voice_filename.push_back(mlib::kPathDelim);
      voice_filename.append(voice_name);
      auto *voice_entry = base_entry->OpenFile(voice_filename);
      if (voice_entry == nullptr || voice_entry->IsOpened() == false) {
        if (voice_entry) {
          delete voice_entry;
          voice_entry = nullptr;
        }
        std::cout << "[Warning] failed to open '" << voice_filename
                  << "'." << std::endl;
        continue;
      }
      std::cout << voice_entry->GetFullPath();
      std::cout << " (Ver. " << voice_entry->GetVersion() << ") -> ";
      const auto file_size = voice_entry->GetSize();
      char *buf = new char[file_size];
      voice_entry->Seek(0, 0);
      voice_entry->Read(file_size, buf);
      char out_filename[32];
      sprintf(out_filename, "%s%cv_%s%04d_%s.ogg",
              dir_name.c_str(), mlib::kPathDelim, lower_product_name.c_str(), ++seq, chara_name.c_str());
      std::ofstream ofs(out_filename);
      std::cout << out_filename;
      ofs.write(buf, file_size);
      ofs.close();
      std::cout << std::endl;
      delete [] buf;
      delete voice_entry;
    }
  }

  delete base_entry;

  return 0;
}
