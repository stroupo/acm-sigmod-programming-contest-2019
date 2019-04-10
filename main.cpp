#include <algorithm>
#include <array>
#include <boost/iostreams/device/mapped_file.hpp>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

struct record {
  std::array<std::uint8_t, 10> key{};
  std::array<std::uint8_t, 90> payload{};
};

// bool operator<(const record& x, const record& y) noexcept {
//   if (uint)
//   return x.key < y.key;
// }

using namespace std;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cout << "usage: " << argv[0] << " <input file> <output file>\n";
    return -1;
  }

  fstream file{argv[1]};
  if (!file) {
    cout << "File could not be opened!\n";
    return -1;
  }

  file.seekg(0, ios::end);
  const auto file_size = file.tellg();

  cout << "file size: " << file_size << " B\n";

  file.close();

  boost::iostreams::mapped_file mfile{argv[1],
                                      boost::iostreams::mapped_file::readwrite};
  cout << "mfile size: " << mfile.size() << " B\n";

  boost::iostreams::mapped_file_params out_params;
  out_params.path = argv[2];
  out_params.flags = boost::iostreams::mapped_file::readwrite;
  out_params.new_file_size = mfile.size();
  // boost::iostreams::mapped_file out_file{
  //     argv[2], boost::iostreams::mapped_file::readwrite, mfile.size()};
  boost::iostreams::mapped_file out_file(out_params);

  const auto first = reinterpret_cast<record*>(mfile.data());
  const auto last = reinterpret_cast<record*>(mfile.data() + mfile.size());
  const auto t0 = chrono::high_resolution_clock::now();
  vector<record> data(distance(first, last));
  copy(first, last, data.begin());
  sort(
      data.begin(), data.end(), [](const auto& x, const auto& y) noexcept {
        // return x.key < y.key;
        const auto xpart1 =
            __builtin_bswap64(*reinterpret_cast<const uint64_t*>(x.key.data()));
        const auto ypart1 =
            __builtin_bswap64(*reinterpret_cast<const uint64_t*>(y.key.data()));
        if (xpart1 < ypart1) return true;
        if (xpart1 > ypart1) return false;
        const auto xpart2 = __builtin_bswap16(
            *reinterpret_cast<const uint16_t*>(x.key.data() + 8));
        const auto ypart2 = __builtin_bswap16(
            *reinterpret_cast<const uint16_t*>(y.key.data() + 8));
        return xpart2 < ypart2;
      });
  copy(data.begin(), data.end(), reinterpret_cast<record*>(out_file.data()));
  const auto t1 = chrono::high_resolution_clock::now();

  cout << "time taken to sort: " << chrono::duration<float>(t1 - t0).count()
       << "\n";
}