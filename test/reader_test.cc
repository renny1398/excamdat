#include <cppunit/extensions/HelperMacros.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mlib/reader.h"

namespace mlib {

class MLibReaderTest : public CPPUNIT_NS::TestFixture {

  CPPUNIT_TEST_SUITE(MLibReaderTest);
  CPPUNIT_TEST(init_test);
  CPPUNIT_TEST(first_read_0_0);
  CPPUNIT_TEST(first_read_0_3);
  CPPUNIT_TEST(first_read_0_cs);
  CPPUNIT_TEST(first_read_0_csp1);
  CPPUNIT_TEST(first_read_0_csm2);
  CPPUNIT_TEST(first_read_0_csm2p1);
  CPPUNIT_TEST(first_read_cspm1_3);
  CPPUNIT_TEST(first_read_cs_3);
  CPPUNIT_TEST(second_read_0_0);
  CPPUNIT_TEST(second_read_in_cache);
  CPPUNIT_TEST(second_read_in_cache2);
  CPPUNIT_TEST(second_read_out_cache);
  CPPUNIT_TEST(second_read_inout_cache);
  CPPUNIT_TEST_SUITE_END();

protected:
  bool create_test_data(const char *filename, size_t size) {
    std::ofstream ofs(filename);
    if (ofs.is_open() == false) {
      return false;
    }

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

  void reset() {
    delete reader_;
    ::lseek(fd_, 0, 0);
    reader_ = new UnencryptedLibReader(fd_);
    init_test();
  }

  void cache_test(size_t cache0_pageno, size_t cache0_pagesize,
                  size_t cache1_pageno, size_t cache1_pagesize) {
    CPPUNIT_ASSERT_EQUAL(cache0_pageno, reader_->page_no_[0]);
    CPPUNIT_ASSERT_EQUAL(cache0_pagesize, reader_->cache_length_[0]);
    CPPUNIT_ASSERT_EQUAL(cache1_pageno, reader_->page_no_[1]);
    CPPUNIT_ASSERT_EQUAL(cache1_pagesize, reader_->cache_length_[1]);
  }

  void read_test(size_t offset, size_t length) {
    reader_->Read(offset, length, buf_);
    off_t tmp_pos = ::lseek(fd_, 0, 1);
    ::lseek(fd_, offset, 0);
    unsigned char* tmp_buf = new unsigned char[length];
    ::read(fd_, tmp_buf, length);
    CPPUNIT_ASSERT_EQUAL(0, ::memcmp(buf_, tmp_buf, length));
    delete [] tmp_buf;
    ::lseek(fd_, tmp_pos, 0);
  }

  std::string src_filename;
  std::string dest_filename;

  int fd_;
  LibReader* reader_;
  unsigned char* buf_;

public:
  MLibReaderTest() : reader_(nullptr) {}

  void setUp() {
    std::srand(std::time(0));
    src_filename.assign("src.txt");
    dest_filename.assign("dest.txt");
    create_test_data(src_filename.c_str(), LibReader::kCacheSize * 3);
    fd_ = ::open(src_filename.c_str(), O_RDONLY);
    reader_ = new UnencryptedLibReader(fd_);
    buf_ = new unsigned char[LibReader::kCacheSize * 3];
  }
  void tearDown() {
    delete [] buf_;
    delete reader_;
    ::close(fd_);
  }

  void init_test() {
    cache_test(SIZE_MAX, 0UL, SIZE_MAX, 0UL);
  }

  void first_read_0_0() {
    reset();
    read_test(0, 0);
    init_test();
  }

  void first_read_0_3() {
    reset();
    read_test(0, 3);
    cache_test(0UL, LibReader::kCacheSize, SIZE_MAX, 0UL);
  }

  void first_read_0_cs() {
    reset();
    read_test(0, LibReader::kCacheSize);
    cache_test(0UL, LibReader::kCacheSize, SIZE_MAX, 0UL);
  }

  void first_read_0_csp1() {
    reset();
    read_test(0, LibReader::kCacheSize + 1);
    cache_test(0UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void first_read_0_csm2() {
    reset();
    read_test(0, LibReader::kCacheSize * 2);
    cache_test(0UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void first_read_0_csm2p1() {
    reset();
    read_test(0, LibReader::kCacheSize * 2 + 1);
    cache_test(2UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void first_read_cspm1_3() {
    init_test();
    read_test(LibReader::kCacheSize - 1, 3);
    cache_test(0UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void first_read_cs_3() {
    init_test();
    read_test(LibReader::kCacheSize, 3);
    cache_test(1UL, LibReader::kCacheSize, SIZE_MAX, 0UL);
  }

  void second_read_0_0() {
    first_read_0_3();
    read_test(0, 0);
    cache_test(0UL, LibReader::kCacheSize, SIZE_MAX, 0UL);
  }

  void second_read_in_cache() {
    first_read_0_csm2();
    read_test(LibReader::kCacheSize, LibReader::kCacheSize);
    cache_test(0UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void second_read_in_cache2() {
    first_read_0_csm2();
    read_test(LibReader::kCacheSize - 3, 6);
    cache_test(0UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void second_read_out_cache() {
    first_read_0_csm2();
    read_test(LibReader::kCacheSize * 2, 3);
    cache_test(2UL, LibReader::kCacheSize, 1UL, LibReader::kCacheSize);
  }

  void second_read_inout_cache() {
    first_read_cs_3();
    read_test(0, LibReader::kCacheSize * 3);
    cache_test(1UL, LibReader::kCacheSize, 2UL, LibReader::kCacheSize);
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(MLibReaderTest);

} // namespace mlib

#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>

int main(/*int argc, char* argv[]*/) {

  CPPUNIT_NS::TestResult controller;

  CPPUNIT_NS::TestResultCollector result;
  controller.addListener( &result );

  CPPUNIT_NS::BriefTestProgressListener progress;
  controller.addListener( &progress );

  CPPUNIT_NS::TestRunner runner;
  runner.addTest( CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest() );
  runner.run( controller );

  CPPUNIT_NS::CompilerOutputter outputter( &result, CPPUNIT_NS::stdCOut() );
  outputter.write();

  return result.wasSuccessful() ? 0 : 1;
}
