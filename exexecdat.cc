#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include "PeLib/PeLib.h"
#include "mlib/camellia.h"
#include "zlib.h"

bool Extract(PeLib::ResourceElement* root) {
  if (root->isLeaf()) return false;
  PeLib::ResourceNode* node = static_cast<PeLib::ResourceNode*>(root);
  // find EEX
  unsigned int i;
  unsigned int uiNamedEntries = node->getNumberOfNamedEntries();
  for (i = 0 ; i < uiNamedEntries; ++i) {
    std::string childname = node->getChildName(i);
    if (childname == "EE\4X") break;
  }
  if (i >= uiNamedEntries) return false;
  if (node->getChild(i)->isLeaf()) return false;
  node = static_cast<PeLib::ResourceNode*>(node->getChild(i));
  // std::cout << "found EEX!" << std::endl;
  // find EEN
  uiNamedEntries = node->getNumberOfNamedEntries();
  for (i = 0 ; i < uiNamedEntries; ++i) {
    std::string childname = node->getChildName(i);
    if (childname == "EE\3N") break;
  }
  if (i >= uiNamedEntries) return false;
  if (node->getChild(i)->isLeaf()) return false;
  node = static_cast<PeLib::ResourceNode*>(node->getChild(i));
  // std::cout << "found EEN!" << std::endl;
  // find 00000000 (leaf)
  unsigned int uiIdEntries = node->getNumberOfIdEntries();
  for (i = 0 ; i < uiIdEntries; ++i) {
    PeLib::dword childOffset = node->getOffsetToChildName(i);
    if (childOffset == 0) break;
  }
  if (i >= uiIdEntries) return false;
  if (node->getChild(i)->isLeaf() == false) return false;
  PeLib::ResourceLeaf* leaf = static_cast<PeLib::ResourceLeaf*>(node->getChild(i));
  // std::cout << "found exec.dat!" << std::endl;

  std::vector<PeLib::byte> data = leaf->getData();

  // for DAF
  unsigned char key[] = { 0x61, 0x67, 0x62, 0x73, 0x76, 0x41, 0x5A, 0x4A, 0x56, 0x42, 0x71, 0x4D, 0x77, 0x7A, 0x6D, 0x45 };
  KEY_TABLE_TYPE kt;
  ::memset(kt, 0, sizeof(kt));
  Camellia_Ekeygen(128, key, kt);
  unsigned int size = data.size();
  std::vector<PeLib::byte> dest(size, 0);
  for (i = 0; i < size; i += CAMELLIA_BLOCK_SIZE) {
    Camellia_DecryptBlock(128, &data[i], kt, &dest[i]);
  }

  unsigned long decomp_size = 1024*1024*10;
  unsigned char *decomp_buff = new unsigned char[decomp_size];
  if (uncompress(decomp_buff, &decomp_size, &dest[0], size) != Z_OK) {
    return false;
  }
  std::ofstream ofs("exec.dat");
  ofs.write(reinterpret_cast<char*>(decomp_buff), decomp_size);
  ofs.close();

  delete [] decomp_buff;
  return true;
}



int main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << "Usage: exexecdat <product-name> <filename>" << std::endl;
    return 0;
  }

  const std::string product_name(argv[1]);
  if (product_name != "DAF") {
    std::cerr << "Sorry, but " << product_name << " is not supported." << std::endl;
    return -1;
  }

  const std::string filename(argv[2]);

  PeLib::PeFile* f = PeLib::openPeFile(filename);
  if (!f) {
    std::cerr << "ERROR: invalid PE file." << std::endl;
    return -2;
  }

  try {
    f->readMzHeader();
    f->readPeHeader();
    f->readResourceDirectory();
  } catch (...) {
    std::cerr << "ERROR: An error occured while reading the file. Maybe the file is not a valid PE file." << std::endl;
    delete f;
    return -3;
  }

  try {
    if (PeLib::getFileType(filename) == PeLib::PEFILE32) {
      if (Extract(f->resDir().getRoot()) == false) {
        std::cerr << "ERROR: cannot extract exec.dat." << std::endl;
        delete f;
        return 1;
      }
    } else {
      std::cerr << "ERROR: invalid PE file." << std::endl;
      delete f;
      return -2;
    }
  } catch (...) {
    std::cerr << "ERROR: An error occured while reading the resource directory. Maybe the directory is invalid." << std::endl;
    delete f;
    return -4;
  }

  delete f;
  return 0;
}
