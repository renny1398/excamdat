#pragma once

/* extractor.h (updated on 2016/12/29)
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

#include "mlib.h"

namespace mlib {

class Extractor {

public:
  Extractor()
    : flatten_(false), mgf2png_(true), webp2png_(true), texcat_(true), texlv_(0), svg_(false), stop_(false) {}

  static void Initialize();
  static void Finalize();

  void Flatten(bool b = true) { flatten_ = b; }
  void EnableMGFToPNG(bool b = true) { mgf2png_ = b; }
  void EnableWebPToPNG(bool b = true) { webp2png_ = b; }
  void EnableTexCat(bool b = true) { texcat_ = b; }
  void EnableSVG(bool b = true) {  svg_ = b; }
  bool SetTexLevel(int lv = 0) {
    if (lv < -1 || 2 < lv) { return false; }
    texlv_ = lv;
    return true;
  }

  bool Extract(MLib* mlib, const std::string &lib_path, const std::string &fs_path);
  void Stop() { stop_ = true; }

protected:
  bool Extract(MLib* mlib, const std::string &fs_path, std::vector<char> &buf);
  bool TexCat(MLib* dzi, MLib* tex_entry, const std::string &fs_path, std::vector<char> &buf);

private:
  static const char kDelim;
  static const char kDelimNotUsed;

  bool flatten_;
  bool mgf2png_;
  bool webp2png_;
  bool texcat_;
  int texlv_;
  bool svg_;

  volatile bool stop_;
};

} // namespace mlib
