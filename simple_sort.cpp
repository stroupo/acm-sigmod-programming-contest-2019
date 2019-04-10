#include <fcntl.h>
#include <omp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

using namespace std;
using namespace std::chrono;

constexpr int record_size = 100;

using record = array<uint8_t, record_size>;

void simple_sort(const string &input_path, const string &output_path) {
  struct stat stats {};
  int fd = open(input_path.c_str(), O_RDONLY);
  fstat(fd, &stats);

  auto size_in_bytes = (size_t)stats.st_size;
  auto *arr_source = static_cast<uint8_t *>(
      mmap64(nullptr, size_in_bytes, PROT_READ, MAP_PRIVATE, fd, 0));

  /*
   * lcm 100 and 4096 is 102400
   */
  uint64_t lcm = 102400 * 100;

  int64_t N = size_in_bytes / lcm;
  const bool is_remainder = (size_in_bytes % lcm) != 0;

  vector<vector<record>> buckets(256 * 256, vector<record>());
  vector<mutex> mutex_vector(256 * 256);

#pragma omp for schedule(dynamic)
  for (int64_t i = 0; i < N + is_remainder; ++i) {
    for (int64_t j = i * lcm; j < min((i + 1) * lcm, size_in_bytes); j += 100) {
      auto const bucked_id =
          __builtin_bswap16(*reinterpret_cast<uint16_t *>(&arr_source[j]));
      lock_guard<mutex> lock(mutex_vector[bucked_id]);
      buckets[bucked_id].push_back(*reinterpret_cast<record *>(&arr_source[j]));
    }
  }

  munmap(arr_source, size_in_bytes);
  close(fd);

  int64_t prefix_sum[256 * 256];
  prefix_sum[0] = 0;
  for (int l = 1; l < 256 * 256; ++l) {
    prefix_sum[l] = prefix_sum[l - 1];
    prefix_sum[l] += buckets[l - 1].size();
  }

  /*
   * output
   */
  int fd2 = open(output_path.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
  lseek(fd2, size_in_bytes - 1, SEEK_SET);
  write(fd2, "", 1);
  auto *arr_sink = (record *)mmap64(nullptr, size_in_bytes,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);

#pragma omp parallel for schedule(dynamic)
  for (int64_t i = 0; i < 256 * 256; ++i) {
    std::sort(buckets[i].begin(), buckets[i].end(),
              [](const record &left, const record &right) {
                return __builtin_bswap64(
                           *reinterpret_cast<const uint64_t *>(&left[2])) <=
                       __builtin_bswap64(
                           *reinterpret_cast<const uint64_t *>(&right[2]));
              });
    for (int32_t j = 0; j < buckets[i].size(); ++j) {
      arr_sink[prefix_sum[i] + j] = buckets[i][j];
    }
    msync(arr_sink + prefix_sum[i], buckets[i].size() * record_size, MS_SYNC);
  }
  munmap(arr_sink, size_in_bytes);
  close(fd2);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "USAGE: " << argv[0] << " [in-file] [outfile]\n";
    std::exit(-1);
  }

  simple_sort(argv[1], argv[2]);

  return 0;
}
