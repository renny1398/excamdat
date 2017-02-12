#include <iostream>
#include <iomanip>
#include <fstream>

const std::string src_filename("data3.dat");
const std::string dst_filename("data3.decrypted.dat");

unsigned char byte_79e098[0x20] = {
  0xA4, 0xA7, 0xA6, 0xA1, 0xA0, 0xA3, 0xA2, 0xAC,
  0xAF, 0xAE, 0xA9, 0xA8, 0xAB, 0xAA, 0xB4, 0xB7,
  0xB6, 0xB1, 0xB0, 0xB3, 0xB2, 0xBC, 0xBF, 0xBE,
  0xB9, 0xB8, 0xBB, 0xBA, 0xA1, 0xA9, 0xB1, 0xB9
};

inline unsigned int rotl(unsigned int data, unsigned int bits) {
 return (data << bits) | (data >> (32 - bits));
}

inline unsigned int rotr(unsigned int data, unsigned int bits) {
 return (data >> bits) | (data << (32 - bits));
}

void decode(size_t offset, const unsigned char *src, unsigned char *dest) {
  unsigned int *dest_dw = reinterpret_cast<unsigned int *>(dest);

  const int d = offset & 0xf;
  const unsigned char val_d = src[d];

  for (int i = 0; i < 0x10; ++i) {
    unsigned char c = src[i];
    if (d != i) {
      c ^= val_d;
    }
    dest[i] = c;
  }

  // std::cout.setf(std::ios::hex, std::ios::basefield);

  size_t block_no = offset >> 4;
  int idx_79e098 = (block_no + 0x0c) & 0x1f;
  int roll2 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  idx_79e098 = (block_no + 0x00) & 0x1f;
  roll2 ^= 0xA5;
  int roll1 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll1 ^= 0xA5;
  unsigned int tmp = rotr(0x79404664, roll1 & 0x1f);
  tmp ^= dest_dw[0];
  dest_dw[0] = rotr(tmp, roll2 & 0x1f);
  // std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << dest[0]<< std::endl;

  idx_79e098 = (block_no + 0x0f) & 0x1f;
  int roll4 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll4 ^= 0xA5;
  idx_79e098 = (block_no + 0x03) & 0x1f;
  int roll3 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll3 ^= 0xA5;
  tmp = rotl(0x772D7635, roll3 & 0x1f);
  tmp ^= dest_dw[1];
  dest_dw[1] = rotl(tmp, roll4 & 0x1f);
  // std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << dest[1]<< std::endl;

  idx_79e098 = (block_no - 0x0e) & 0x1f;
  int roll6 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll6 ^= 0xA5;
  idx_79e098 = (block_no + 0x06) & 0x1f;
  int roll5 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll5 ^= 0xA5;
  tmp = rotr(0x346B7230, roll5 & 0x1f);
  tmp ^= dest_dw[2];
  dest_dw[2] = rotr(tmp, roll6 & 0x1f);
  // std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << dest[2]<< std::endl;

  idx_79e098 = (block_no - 0x0b) & 0x1f;
  int roll8 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll8 ^= 0xA5;
  idx_79e098 = (block_no + 0x09) & 0x1f;
  int roll7 = static_cast<unsigned int>(byte_79e098[idx_79e098]);
  roll7 ^= 0xA5;
  tmp = rotl(0x6E7E7935, roll7 & 0x1f);
  tmp ^= dest_dw[3];
  dest_dw[3] = rotl(tmp, roll8 & 0x1f);
  // std::cout << "0x" << std::right << std::setfill('0') << std::setw(8) << dest[3]<< std::endl;

  // std::cout.setf(std::ios::dec, std::ios::basefield);
}

int main() {
  std::ifstream ifs(src_filename, std::ios_base::binary);
  if (ifs.is_open() == false) {
    std::cerr << "ERROR: failed to open '" << src_filename << "'." << std::endl;
    return -1;
  }
  std::ofstream ofs(dst_filename, std::ios_base::binary);
  if (ofs.is_open() == false) {
    std::cerr << "ERROR: failed to open '" << dst_filename << "'." << std::endl;
    return -1;
  }

  ifs.seekg(0, std::ios_base::seek_dir::end);
  const size_t len = ifs.tellg();
  ifs.seekg(0, std::ios_base::seek_dir::beg);

  unsigned char raw[16];
  unsigned char decrypted[16];
  for (size_t i = 0; i < len; i += 16) {
    ifs.read(reinterpret_cast<char *>(raw), 16);
    decode(i, raw, decrypted);
    ofs.write(reinterpret_cast<char *>(decrypted), 16);
  }

  ofs.close();
  ifs.close();
  return 0;
}
