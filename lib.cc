/* lib.cc (updated on 2016/04/03)
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

#include <iostream>
#include <cassert>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>

#include "lib.h"
#include "camellia.h"

using namespace std;


namespace {

// DIE series
const unsigned char KEY_DIE[16] = { 0 };  // same as DDW, DAF, DSG, DCS
const unsigned char* const KEY_DDW = KEY_DIE;
const unsigned char* const KEY_DAF = KEY_DIE;
const unsigned char* const KEY_DSG = KEY_DIE;
const unsigned char* const KEY_DCS = KEY_DIE;

// Version 1
const unsigned char KEY_SCC[] = { 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04,
                                  0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0e, 0x0d, 0x0c };
const unsigned char KEY_ARS[] = "T6ujfq9gVRxQcWXJ";
const unsigned char KEY_TAP[] = "QM65vnGuBKocFULH";
const unsigned char KEY_DCQ[] = "qcGblQSEdXunaMiF";
const unsigned char KEY_SEL[] = "]L_Houfw}DT{U-;t";
const unsigned char KEY_MAJ[] = { 0x60, 0x75, 0xa4, 0xbc, 0x9b, 0xd1, 0x2f, 0x83,
                                  0x48, 0xda, 0x79, 0x01, 0x06, 0xfc, 0x35, 0x26 };
const unsigned char KEY_PL[]  = "1Ipf5YzVWNQGxY5K";
const unsigned char KEY_KKK_T[] = "XogRr2FjLW0waAuW";
const unsigned char KEY_VER[] = "Z/1wqvaX;Emj@)e+";
const unsigned char KEY_KKK[] = "{p.nYDyNjFh/D@}?";
const unsigned char KEY_KNY[] = "kJ71DvMW5Jzijls0";
const unsigned char KEY_ZRI[] = "fqlqy9tge8B3alyt";
const unsigned char KEY_SPK[] = "ovld8dv4uqsnawtp";
const unsigned char KEY_KCS[] = "4v$sd@q(k6?x12/p";

// Version 2
const unsigned char KEY_BRA[] = "2jtaovbfAjii9f1m";
const unsigned char KEY_ERA[] = "4eDbc9w8E32bBgs6";
const unsigned char KEY_SSG[] = "7r3iBgm+z26!qy9a";
const unsigned char KEY_SLV[] = "E+Aw@Cxbs-usgcw)";
const unsigned char KEY_SGB[] = "(yD@pig4+k6pe-nq";
const unsigned char KEY_IKB[] = "Cwdb(5A4iF+Dx@xe";


struct KEY_INFO {
  char product[8];
  const unsigned char* key;
};

const KEY_INFO KEYS[] = {
  { "DIE", KEY_DIE },
  { "SCC", KEY_SCC },
  { "ARS", KEY_ARS },
  { "TAP", KEY_TAP },
  { "DCQ", KEY_DCQ },
  { "DDW", KEY_DDW },
  { "DAF", KEY_DAF },
  { "DSG", KEY_DSG },
  { "SEL", KEY_SEL },
  { "MAJ", KEY_MAJ },
//{ "PL",  KEY_PL  },
  { "KKK_T", KEY_KKK_T },
  { "VER", KEY_VER },
  { "KKK", KEY_KKK },
  { "KNY", KEY_KNY },
  { "ZRI", KEY_ZRI },
  { "SPK", KEY_SPK },
  { "KCS", KEY_KCS },
  { "BRA", KEY_BRA },
  { "ERA", KEY_ERA },
  { "SSG", KEY_SSG },
  { "SLV", KEY_SLV },
  { "SGB", KEY_SGB },
  { "IKB", KEY_IKB },
  { "", NULL }
};



unsigned int rotl(unsigned int data, unsigned int bits) {
 return (data << bits) | (data >> (32 - bits));
}

unsigned int rotr(unsigned int data, unsigned int bits) {
 return (data >> bits) | (data << (32 - bits));
}

} // namespace



Lib::Lib(const string &file_name) : ifs_(file_name.c_str(), ios::in | ios::binary) {}

EncryptedLib::EncryptedLib(const std::string& file_name, const KEY_TABLE_TYPE keyTable)
  : Lib(file_name) {
  ::memcpy(key_table_, keyTable, sizeof(KEY_TABLE_TYPE));
}

Libu_t::Libu_t(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBUHDR &header)
  : EncryptedLib(file_name, keyTable) {

}



Libp_t::Libp_t(const std::string& file_name, const KEY_TABLE_TYPE keyTable, const LIBPHDR &header)
  : EncryptedLib(file_name, keyTable), base_offset_(16) {

  entry1_count_ = header.entry1_count;
  entry2_count_ = header.entry2_count;

  entries1_ = new LIBPENTRY1[entry1_count_];
  unsigned int read_size = sizeof(LIBPENTRY1) * entry1_count_;
  Read(entries1_, read_size, base_offset_);
  base_offset_ += read_size;

  cout << "Entry1 count = " << entry1_count_ << endl;

  entries2_ = new LIBPENTRY2[entry2_count_];
  read_size = sizeof(LIBPENTRY2) * entry2_count_;
  Read(entries2_, read_size, base_offset_);
  base_offset_ += read_size;

  base_offset_ = (base_offset_ + 1023) & ~1023;
}


Libp_t::~Libp_t() {
  delete [] entries2_;
  delete [] entries1_;
}




inline unsigned int mutate_value(unsigned int x) {
  return (rotl(x, 8) & 0x00ff00ff) | (rotr(x, 8) & 0xff00ff00);
}

inline void mutate_block(const unsigned int *src, unsigned int *mutated) {
  mutated[0] = mutate_value(src[0]);
  mutated[1] = mutate_value(src[1]);
  mutated[2] = mutate_value(src[2]);
  mutated[3] = mutate_value(src[3]);
}



void Lib::DecryptBlock(std::ifstream& ifs, const KEY_TABLE_TYPE keyTable, void* buff, unsigned int len, unsigned int offset) {

  static const unsigned int kBlockLength = 16;


  if (keyTable != NULL) {
    const unsigned int block_pad = offset % kBlockLength;
    const unsigned int aligned_len = (len + block_pad + 0x0f) & ~0x0f;
    unsigned char* aligned_buff = new unsigned char[aligned_len];

    unsigned char* block = aligned_buff;
    const unsigned int block_count = aligned_len / kBlockLength;

    offset -= block_pad;
    ifs.seekg(offset);
    ifs.read((char*)aligned_buff, aligned_len);

    for (unsigned int i = block_count; 0 < i; i--) {

      unsigned char block_tmp[16];
      const unsigned int roll_bits = ((offset >> 4) & 0x0f) + 16;

      unsigned int* p = (unsigned int*)block;
      unsigned int* q = (unsigned int*)block_tmp;

      // mutate_block(p, p);
      q[0] = rotl(p[0], roll_bits);
      q[1] = rotr(p[1], roll_bits);
      q[2] = rotl(p[2], roll_bits);
      q[3] = rotr(p[3], roll_bits);
      // mutate_block(q, q);

      Camellia_DecryptBlock(128, block_tmp, keyTable, block);

      block  += kBlockLength;
      offset += kBlockLength;
    }

    ::memcpy(buff, aligned_buff + block_pad, len);

    delete [] aligned_buff;
    return;
  }

  ifs.seekg(offset);
  ifs.read((char*)buff, len);
}


void EncryptedLib::Read(void *buff, unsigned int len, unsigned int offset) {
  Lib::DecryptBlock(this->ifs_, this->key_table_, buff, len, offset);
}



Lib* Lib::Open(const string& file_name) {

  ifstream ifs(file_name.c_str(), ios::in | ios::binary);

  if (ifs.is_open() == false) {
    cerr << "Error: file is not found." << endl;
    return NULL;
  }

  KEY_TABLE_TYPE keyTable;
  LIBUHDR hdr;

  // search a key that corresponds to the data
  for (int i = 0; KEYS[i].key != NULL; i++) {
    Camellia_Ekeygen(128, KEYS[i].key, keyTable);
    DecryptBlock(ifs, keyTable, &hdr, 16, 0);
    if (hdr.signature[0] == 'L' && hdr.signature[1] == 'I' && hdr.signature[2] == 'B') {
      cout << "Camellia key type: " << KEYS[i].product << endl;
      break;
    }
  }

  if (hdr.signature[0] != 'L' || hdr.signature[1] != 'I' || hdr.signature[2] != 'B') {
    cerr << "Error: this file is not a Cammelia data file." << endl;
    return 0;
  }

  ifs.close();

  Lib* ret = 0;

  if (hdr.signature[3] == 'P') {
    ret = new Libp_t(file_name, keyTable, (LIBPHDR&)hdr);
    return ret;
  }

  if (hdr.signature[3] == 'U') {
    ret = new Libu_t(file_name, keyTable, hdr);
    return ret;
  }

  return 0;
}




void Libu_t::Extract(bool flatten, const string& prefix, unsigned int offset, unsigned int length) {

  const char dir_delim = flatten && prefix != "." ? '-' : '/';

  LIBUHDR hdr;
  Read(&hdr, sizeof(hdr), offset);

  if (hdr.signature[0] == 'L' && hdr.signature[1] == 'I' && hdr.signature[2] == 'B') {
    cout << "Format: LIBU" << endl;

    LIBUENTRY* entries = new LIBUENTRY[hdr.entry_count];
    Read(entries, sizeof(LIBUENTRY) * hdr.entry_count, offset + sizeof(hdr));

    if (flatten == false) {
      ::mkdir(prefix.c_str(), 0755);
    }

    for (unsigned int i = 0; i < hdr.entry_count; i++) {
      char mb_file_name[261];
      ::wcstombs(mb_file_name, entries[i].file_name, 32);

      cout << mb_file_name << endl;
      // cout << entries[i].length << endl;

      Extract(flatten, prefix + dir_delim + mb_file_name, offset + entries[i].length, entries[i].length);
    }

    delete [] entries;
    return;
  }

  char* buff = new char[length];
  Read(buff, length, offset);

  bool is_mgf = length >= 7 && !::memcmp(buff, "\x4d\x61\x6c\x69\x65\x47\x46\0", 8);

  string file_name(prefix);
  if (is_mgf) {
    string::size_type ext_pos = prefix.find_last_of(".mgf");
    file_name = prefix.substr(0, ext_pos) + ".png";
    ::memcpy(buff, "\x89PNG\x0D\x0A\x1A\x0A", 8);
  }

  if (IsVerbose()) {
    cout << "Extracting '" << file_name << "'..." << endl;
  }
  ofstream ofs(file_name.c_str(), ios::out | ios::binary);
  ofs.write(buff, length);
}



void Libu_t::Extract(bool flatten) {
  Extract(flatten, ".", 0, 0);
}


void Libp_t::Extract(bool flatten, const string &prefix, unsigned int entry1_offset, unsigned int entry_count) {

  const char dir_delim = flatten && prefix != "." ? '-' : '/';

  if (flatten == false && prefix != "." && prefix != "./") {
    cout << "Making a directory '" << prefix << "'" << endl;
    ::mkdir(prefix.c_str(), 0755);
  }

  LIBPENTRY1* curr = entries1_ + entry1_offset;
  // cout << entry1_offset << endl;
  // cout << entry_count << endl;

  for (unsigned int i = 0; i < entry_count; i++) {
    string file_name = prefix + dir_delim + curr[i].file_name;

    cout << "File name = " << file_name << endl;
    cout << curr[i].file_name << endl;
    // cout << entries1_[0].length << endl;

    if (curr[i].flags & LIBPENTRY1::kFlagFile) {
      char* buff = new char[curr[i].length];
      unsigned int offset = base_offset_ + entries2_[curr[i].offset_index].offset * 1024;
      Read(buff, curr[i].length, offset);

      ofstream ofs(file_name.c_str(), ios::out | ios::binary);
      if (IsVerbose()) {
        cout << "Extracting '" << file_name << "'..." << endl;
      }
      ofs.write(buff, curr[i].length);

      delete [] buff;
      continue;
    }

    Extract(flatten, file_name, curr[i].offset_index, curr[i].length);
  }
}


void Libp_t::Extract(bool flatten) {
  Extract(flatten, ".", 0, 1);
}
