#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include "PeLib/PeLib.h"
#include "mlib/camellia.h"
#include "zlib.h"

template<typename T>
std::string convert(T x) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << x;
  return ss.str();
}

template<int bits>
void dumpResourceTree(const std::string& name, PeLib::ResourceElement* elem, std::string pad, const PeLib::PeHeaderT<bits>& peh) {
  bool isLeaf = elem->isLeaf();

  std::cout << std::endl;
  std::cout << pad << "ResourceElement:      " << name << std::endl;
  std::cout << pad << "Type:                 " << (isLeaf ? "Leaf" : "Node") << std::endl;
  std::cout << pad << "RVA:                  " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << elem->getElementRva() << std::endl;
  std::cout << pad << "Offset:               " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << peh.rvaToOffset(elem->getElementRva()) << std::endl;

  if (isLeaf) {
    PeLib::ResourceLeaf* leaf = static_cast<PeLib::ResourceLeaf*>(elem);

    std::cout << pad << "OffsetToData:         " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << leaf->getOffsetToData() << std::endl;
    std::cout << pad << "Size:                 " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << leaf->getSize() << std::endl;
    std::cout << pad << "CodePage:             " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << leaf->getCodePage() << std::endl;
    std::cout << pad << "Reserved:             " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << leaf->getReserved() << std::endl;
  } else{
    PeLib::ResourceNode* node = static_cast<PeLib::ResourceNode*>(elem);

    unsigned int uiNamedEntries = node->getNumberOfNamedEntries();
    unsigned int uiIdEntries = node->getNumberOfIdEntries();

    std::cout << pad << "Characteristics:      " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << node->getCharacteristics() << std::endl;
    std::cout << pad << "TimeDateStamp:        " << std::setw(8) << std::setfill('0') << std::uppercase << std::hex << node->getTimeDateStamp() << std::endl;
    std::cout << pad << "MajorVersion:         " << std::setw(4) << std::setfill('0') << std::uppercase << std::hex << node->getMajorVersion() << std::endl;
    std::cout << pad << "MinorVersion:         " << std::setw(4) << std::setfill('0') << std::uppercase << std::hex << node->getMinorVersion() << std::endl;
    std::cout << pad << "NumberOfNamedEntries: " << std::setw(4) << std::setfill('0') << std::uppercase << std::hex << uiNamedEntries << std::endl;
    std::cout << pad << "NumberOfIdEntries:    " << std::setw(4) << std::setfill('0') << std::uppercase << std::hex << uiIdEntries << std::endl;

    for (unsigned int i=0;i<uiNamedEntries;i++) {
      PeLib::dword childOffset = node->getOffsetToChildName(i);
      PeLib::dword dataOffset = node->getOffsetToChildData(i);
      std::string childname = node->getChildName(i);

      dumpResourceTree(childname + " (Offset: " + convert(childOffset) + ")", node->getChild(i), pad + "\t", peh);
    }

    for (unsigned int i=0;i<uiIdEntries;i++) {
      PeLib::dword childOffset = node->getOffsetToChildName(i);
      PeLib::dword dataOffset = node->getOffsetToChildData(i);

      dumpResourceTree(convert(childOffset), node->getChild(i), pad + "\t", peh);
    }
  }
}

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
  // unsigned char key[] = { 0x8C, 0x51, 0x90, 0xC2, 0x82, 0xCC, 0x8B, 0xF3, 0x82, 0xF0, 0x89, 0x7A, 0x82, 0xA6, 0x82, 0xC4 };
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
  if (product_name != "DAF" && product_name != "-t") {
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
      if (product_name == "-t") {
        dumpResourceTree("Root", f->resDir().getRoot(), "", static_cast<PeLib::PeFileT<32>*>(f)->peHeader());
      } else if (Extract(f->resDir().getRoot()) == false) {
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
