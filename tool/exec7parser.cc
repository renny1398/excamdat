/* exec7parser.cc (updated on 2017/06/28)
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

#include "mlib/exec.h"
#include <iostream>

int main(int argc, char **argv) {

  if (argc < 3) {
    std::cout << "Usage: exec7parser <product_name> <exec.dat>" << std::endl;
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

  mlib::VersionedFile file(argv[2], argv[1]);
  if (file.IsOpened() == false) {
    std::cerr << "ERROR: failed to open '" << argv[2] << "'." << std::endl;
    return -1;
  }

  mlib::Exec exec(&file);

  mlib::ExecTextToASText to_as;
  std::string as_out = exec.ParseText(&to_as);
  std::ofstream as_ofs("scenario.txt");
  as_ofs << as_out;
  as_ofs.close();

  mlib::ExecTextToXhtml to_xhtml;
  std::string xhtml_out = exec.ParseText(&to_xhtml);
  std::ofstream xhtml_ofs("exec.xhtml");
  xhtml_ofs << to_xhtml.GetHeader();
  xhtml_ofs << xhtml_out;
  xhtml_ofs << to_xhtml.GetFooter();
  xhtml_ofs.close();

  return 0;
}
