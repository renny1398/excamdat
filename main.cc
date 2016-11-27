/* main.cc (updated on 2015/09/26)
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


////////////////////////////////////////////////////////////////////////
// excamdat v0.01 - Camellia-encrypted archive extractor
// Author : renny1398
////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <string>
#include <locale.h>
#include "lib.h"

using namespace std;


struct Params {
  string product_name;
  string input_file_name;
  string output_dir;
  bool is_verbose;
  bool only_decrypts;
  Params() : is_verbose(false), only_decrypts(false) {}
};


void print_usage() {
  cout << "Usage: excammdat <product name> <input data> [-o output dir] [--verbose] [--only-decrypt]" << endl;
}


bool get_params(int argc, const char** argv, Params* params) {

  bool flag_output_dir = false;

  for (int i = 1; i < argc; i++) {
    const string arg(argv[i]);

    if (flag_output_dir == true) {
      params->output_dir = arg;
      flag_output_dir = false;
      continue;
    }

    if (arg == "-v" || arg == "--verbose") {
      params->is_verbose = true;
      continue;
    }
    if (arg == "--only-decrypt") {
      params->only_decrypts = true;
      continue;
    }
    if (arg == "-o") {
      flag_output_dir = true;
      continue;
    }

    if (params->product_name.empty()) {
      params->product_name = arg;
    } else {
      params->input_file_name = arg;
    }
  }

  if (flag_output_dir == true) {
    cerr << "Error: not specify an output directory" << endl;
    return false;
  }

  return true;
}

#include "camellia.h"
#include <fstream>

int Decrypt(const string& filename, const string& product) {
  ifstream ifs(filename);
  if (ifs.is_open() == false) {
    cerr << "Error: file is not found." << endl;
    return -1;
  }
  
  const string output(filename + ".decrypted");
  ofstream ofs(output, ios::binary);
  if (ofs.is_open() == false) {
    cerr << "Error: output file cannot be opened.";
    return -1;
  }
  
  size_t len = (size_t)ifs.seekg(0, ios::end).tellg();
  ifs.seekg(0, ios::beg);
  char* buff = new char[len];
  
  KEY_TABLE_TYPE keyTable;
  Lib::GetKeyTable(product, keyTable);
  
  Lib::DecryptBlock(ifs, keyTable, buff, len, 0);
  ofs.write(buff, len);

  delete [] buff;
  ofs.close();
  ifs.close();

  cout << "Decrypted '" << filename << "' and saved as '" << output << "'." << endl;
  
  return 0;
}


int main(int argc, const char** argv) {

  if (argc <= 1) {
    print_usage();
    return 0;
  }

  Params params;
  if (get_params(argc, argv, &params) == false) {
    return -1;
  }

  setlocale(LC_CTYPE, "jpn");

  if (params.only_decrypts) {
    return Decrypt(params.input_file_name, params.product_name);
  }

  Lib* lib = Lib::Open(params.input_file_name, params.product_name);
  if (lib == NULL) return -1;

  lib->Verbose();

  cout << "Start extracting." << endl;
  lib->Extract(false);

  delete lib;

  return 0;
}
