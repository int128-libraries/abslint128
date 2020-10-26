#include <cstdio>
#include <stdint.h>

#include "abslint128.h"

using namespace absl;

int main(int argc, char ** argv)
{
  uint128_t x = (uint128_t) 1 << 120;
#if 0
  if (argc == 2)
    x = string_to_uint128((std::string)argv[1]);
#endif
  #pragma omp parallel for
  for (uint64_t v = 2; v < 1u << 24; v++) {
    uint128_t r;
    uint128_t y;
    uint128_t::DivMod(x, v, &y, &r);
    uint128_t z = y * v + r;

    if (z != x)
      fprintf(stderr, "Error : y (%s) * v (%lu) + r (%lu) != x (%s)\n",
        uint128_t::ToString(y).c_str(), v, r, uint128_t::ToString(x).c_str());
  }

  printf("Done!\n");

  return 0;
}
