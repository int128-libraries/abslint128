#include <cstdio>
#include <stdint.h>

#include "abslint128.h"

using namespace absl;

int main(int argc, char ** argv)
{
  int128_t x = (int128_t) 1 << 120;
#if 0
  if (argc == 2)
    x = string_to_int128((std::string)argv[1]);
#endif
  #pragma omp parallel for
  for (int64_t v = 2; v < 1 << 24; v++) {
    int128_t r;
    int128_t y;
    int128_t::DivMod(x, v, &y, &r);
    int128_t z = y * v + r;

    if (z != x)
      fprintf(stderr, "Error : y (%s) * v (%ld) + r (%ld) != x (%s)\n",
        int128_t::ToString(y).c_str(), v, r, int128_t::ToString(x).c_str());
  }

  printf("Done!\n");

  return 0;
}
