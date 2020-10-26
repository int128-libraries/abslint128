// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "abslint128.h"

#include <stddef.h>

#include <cassert>
#include <iomanip>
#include <ostream>  // NOLINT(readability/streams)
#include <sstream>
#include <string>
#include <type_traits>

namespace absl {

ABSL_DLL const uint128_t kuint128_tmax = MakeUint128(
    std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max());

namespace {

ABSL_ATTRIBUTE_ALWAYS_INLINE int CountLeadingZeros64Slow(uint64_t n) {
  int zeroes = 60;
  if (n >> 32) {
    zeroes -= 32;
    n >>= 32;
  }
  if (n >> 16) {
    zeroes -= 16;
    n >>= 16;
  }
  if (n >> 8) {
    zeroes -= 8;
    n >>= 8;
  }
  if (n >> 4) {
    zeroes -= 4;
    n >>= 4;
  }
  return "\4\3\2\2\1\1\1\1\0\0\0\0\0\0\0"[n] + zeroes;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE int CountLeadingZeros64(uint64_t n) {
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
  // MSVC does not have __buitin_clzll. Use _BitScanReverse64.
  unsigned long result = 0;  // NOLINT(runtime/int)
  if (_BitScanReverse64(&result, n)) {
    return 63 - result;
  }
  return 64;
#elif defined(_MSC_VER) && !defined(__clang__)
  // MSVC does not have __buitin_clzll. Compose two calls to _BitScanReverse
  unsigned long result = 0;  // NOLINT(runtime/int)
  if ((n >> 32) &&
      _BitScanReverse(&result, static_cast<unsigned long>(n >> 32))) {
    return 31 - result;
  }
  if (_BitScanReverse(&result, static_cast<unsigned long>(n))) {
    return 63 - result;
  }
  return 64;
#elif defined(__GNUC__) || defined(__clang__)
  // Use __builtin_clzll, which uses the following instructions:
  //  x86: bsr
  //  ARM64: clz
  //  PPC: cntlzd
  static_assert(sizeof(unsigned long long) == sizeof(n),  // NOLINT(runtime/int)
                "__builtin_clzll does not take 64-bit arg");

  // Handle 0 as a special case because __builtin_clzll(0) is undefined.
  if (n == 0) {
    return 64;
  }
  return __builtin_clzll(n);
#else
  return CountLeadingZeros64Slow(n);
#endif
}

// Returns the 0-based position of the last set bit (i.e., most significant bit)
// in the given uint128_t. The argument is not 0.
//
// For example:
//   Given: 5 (decimal) == 101 (binary)
//   Returns: 2
inline ABSL_ATTRIBUTE_ALWAYS_INLINE int Fls128(uint128_t n) {
  if (uint64_t hi = Uint128High64(n)) {
    ABSL_INTERNAL_ASSUME(hi != 0);
    return 127 - CountLeadingZeros64(hi);
  }
  const uint64_t low = Uint128Low64(n);
  ABSL_INTERNAL_ASSUME(low != 0);
  return 63 - CountLeadingZeros64(low);
}

template <typename T>
uint128_t MakeUint128FromFloat(T v) {
  static_assert(std::is_floating_point<T>::value, "");

  // Rounding behavior is towards zero, same as for built-in types.

  // Undefined behavior if v is NaN or cannot fit into uint128_t.
  assert(std::isfinite(v) && v > -1 &&
         (std::numeric_limits<T>::max_exponent <= 128 ||
          v < std::ldexp(static_cast<T>(1), 128)));

  if (v >= std::ldexp(static_cast<T>(1), 64)) {
    uint64_t hi = static_cast<uint64_t>(std::ldexp(v, -64));
    uint64_t lo = static_cast<uint64_t>(v - std::ldexp(static_cast<T>(hi), 64));
    return MakeUint128(hi, lo);
  }

  return MakeUint128(0, static_cast<uint64_t>(v));
}

#if defined(__clang__) && !defined(__SSE3__)
// Workaround for clang bug: https://bugs.llvm.org/show_bug.cgi?id=38289
// Casting from long double to uint64_t is miscompiled and drops bits.
// It is more work, so only use when we need the workaround.
uint128_t MakeUint128FromFloat(long double v) {
  // Go 50 bits at a time, that fits in a double
  static_assert(std::numeric_limits<double>::digits >= 50, "");
  static_assert(std::numeric_limits<long double>::digits <= 150, "");
  // Undefined behavior if v is not finite or cannot fit into uint128_t.
  assert(std::isfinite(v) && v > -1 && v < std::ldexp(1.0L, 128));

  v = std::ldexp(v, -100);
  uint64_t w0 = static_cast<uint64_t>(static_cast<double>(std::trunc(v)));
  v = std::ldexp(v - static_cast<double>(w0), 50);
  uint64_t w1 = static_cast<uint64_t>(static_cast<double>(std::trunc(v)));
  v = std::ldexp(v - static_cast<double>(w1), 50);
  uint64_t w2 = static_cast<uint64_t>(static_cast<double>(std::trunc(v)));
  return (static_cast<uint128_t>(w0) << 100) | (static_cast<uint128_t>(w1) << 50) |
         static_cast<uint128_t>(w2);
}
#endif  // __clang__ && !__SSE3__
}  // namespace

uint128_t::uint128_t(float v) : uint128_t(MakeUint128FromFloat(v)) {}
uint128_t::uint128_t(double v) : uint128_t(MakeUint128FromFloat(v)) {}
uint128_t::uint128_t(long double v) : uint128_t(MakeUint128FromFloat(v)) {}

// Long division/modulo for uint128_t implemented using the shift-subtract
// division algorithm adapted from:
// https://stackoverflow.com/questions/5386377/division-without-using
void uint128_t::DivMod(uint128_t dividend, uint128_t divisor, uint128_t* quotient_ret,
                       uint128_t* remainder_ret) {
  assert(divisor != 0);

  if (divisor > dividend) {
    *quotient_ret = 0;
    *remainder_ret = dividend;
    return;
  }

  if (divisor == dividend) {
    *quotient_ret = 1;
    *remainder_ret = 0;
    return;
  }

  uint128_t denominator = divisor;
  uint128_t quotient = 0;

  // Left aligns the MSB of the denominator and the dividend.
  const int shift = Fls128(dividend) - Fls128(denominator);
  denominator <<= shift;

  // Uses shift-subtract algorithm to divide dividend by denominator. The
  // remainder will be left in dividend.
  for (int i = 0; i <= shift; ++i) {
    quotient <<= 1;
    if (dividend >= denominator) {
      dividend -= denominator;
      quotient |= 1;
    }
    denominator >>= 1;
  }

  *quotient_ret = quotient;
  *remainder_ret = dividend;
}

uint128_t operator/(uint128_t lhs, uint128_t rhs) {
#if defined(ABSL_HAVE_INTRINSIC_INT128)
  return static_cast<unsigned __int128>(lhs) /
         static_cast<unsigned __int128>(rhs);
#else  // ABSL_HAVE_INTRINSIC_INT128
  uint128_t quotient = 0;
  uint128_t remainder = 0;
  uint128_t::DivMod(lhs, rhs, &quotient, &remainder);
  return quotient;
#endif  // ABSL_HAVE_INTRINSIC_INT128
}
uint128_t operator%(uint128_t lhs, uint128_t rhs) {
#if defined(ABSL_HAVE_INTRINSIC_INT128)
  return static_cast<unsigned __int128>(lhs) %
         static_cast<unsigned __int128>(rhs);
#else  // ABSL_HAVE_INTRINSIC_INT128
  uint128_t quotient = 0;
  uint128_t remainder = 0;
  uint128_t::DivMod(lhs, rhs, &quotient, &remainder);
  return remainder;
#endif  // ABSL_HAVE_INTRINSIC_INT128
}

std::string uint128_t::ToFormattedString(uint128_t v, std::ios_base::fmtflags flags) {
  // Select a divisor which is the largest power of the base < 2^64.
  uint128_t div;
  int div_base_log;
  switch (flags & std::ios::basefield) {
    case std::ios::hex:
      div = 0x1000000000000000;  // 16^15
      div_base_log = 15;
      break;
    case std::ios::oct:
      div = 01000000000000000000000;  // 8^21
      div_base_log = 21;
      break;
    default:  // std::ios::dec
      div = 10000000000000000000u;  // 10^19
      div_base_log = 19;
      break;
  }

  // Now piece together the uint128_t representation from three chunks of the
  // original value, each less than "div" and therefore representable as a
  // uint64_t.
  std::ostringstream os;
  std::ios_base::fmtflags copy_mask =
      std::ios::basefield | std::ios::showbase | std::ios::uppercase;
  os.setf(flags & copy_mask, copy_mask);
  uint128_t high = v;
  uint128_t low;
  uint128_t::DivMod(high, div, &high, &low);
  uint128_t mid;
  uint128_t::DivMod(high, div, &high, &mid);
  if (Uint128Low64(high) != 0) {
    os << Uint128Low64(high);
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
    os << Uint128Low64(mid);
    os << std::setw(div_base_log);
  } else if (Uint128Low64(mid) != 0) {
    os << Uint128Low64(mid);
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  os << Uint128Low64(low);
  return os.str();
}

std::string uint128_t::ToString(uint128_t v) {
  return uint128_t::ToFormattedString(v);
}

std::ostream& operator<<(std::ostream& os, uint128_t v) {
  std::ios_base::fmtflags flags = os.flags();
  std::string rep = uint128_t::ToFormattedString(v, flags);

  // Add the requisite padding.
  std::streamsize width = os.width(0);
  if (static_cast<size_t>(width) > rep.size()) {
    std::ios::fmtflags adjustfield = flags & std::ios::adjustfield;
    if (adjustfield == std::ios::left) {
      rep.append(width - rep.size(), os.fill());
    } else if (adjustfield == std::ios::internal &&
               (flags & std::ios::showbase) &&
               (flags & std::ios::basefield) == std::ios::hex && v != 0) {
      rep.insert(2, width - rep.size(), os.fill());
    } else {
      rep.insert(0, width - rep.size(), os.fill());
    }
  }

  return os << rep;
}

namespace {

uint128_t UnsignedAbsoluteValue(int128_t v) {
  // Cast to uint128_t before possibly negating because -Int128Min() is undefined.
  return Int128High64(v) < 0 ? -uint128_t(v) : uint128_t(v);
}

}  // namespace

#if !defined(ABSL_HAVE_INTRINSIC_INT128)
namespace {

template <typename T>
int128_t MakeInt128FromFloat(T v) {
  // Conversion when v is NaN or cannot fit into int128_t would be undefined
  // behavior if using an intrinsic 128-bit integer.
  assert(std::isfinite(v) && (std::numeric_limits<T>::max_exponent <= 127 ||
                              (v >= -std::ldexp(static_cast<T>(1), 127) &&
                               v < std::ldexp(static_cast<T>(1), 127))));

  // We must convert the absolute value and then negate as needed, because
  // floating point types are typically sign-magnitude. Otherwise, the
  // difference between the high and low 64 bits when interpreted as two's
  // complement overwhelms the precision of the mantissa.
  uint128_t result = v < 0 ? -MakeUint128FromFloat(-v) : MakeUint128FromFloat(v);
  return MakeInt128(int128_t_internal::BitCastToSigned(Uint128High64(result)),
                    Uint128Low64(result));
}

}  // namespace

int128_t::int128_t(float v) : int128_t(MakeInt128FromFloat(v)) {}
int128_t::int128_t(double v) : int128_t(MakeInt128FromFloat(v)) {}
int128_t::int128_t(long double v) : int128_t(MakeInt128FromFloat(v)) {}

void int128_t::DivMod(int128_t dividend, int128_t divisor, int128_t* quotient_ret,
                      int128_t* remainder_ret) {
  assert(dividend != Int128Min() || divisor != -1);  // UB on two's complement.

  uint128_t quotient = 0;
  uint128_t remainder = 0;
  uint128_t::DivMod(UnsignedAbsoluteValue(dividend), UnsignedAbsoluteValue(divisor),
             &quotient, &remainder);
  if ((Int128High64(dividend) < 0) != (Int128High64(divisor) < 0)) quotient = -quotient;
  *quotient_ret = MakeInt128(int128_t_internal::BitCastToSigned(Uint128High64(quotient)),
                             Uint128Low64(quotient));
  if (Int128High64(dividend) < 0) remainder = -remainder;
  *remainder_ret = MakeInt128(int128_t_internal::BitCastToSigned(Uint128High64(remainder)),
                              Uint128Low64(remainder));
}

int128_t operator/(int128_t lhs, int128_t rhs) {
  assert(lhs != Int128Min() || rhs != -1);  // UB on two's complement.

  uint128_t quotient = 0;
  uint128_t remainder = 0;
  uint128_t::DivMod(UnsignedAbsoluteValue(lhs), UnsignedAbsoluteValue(rhs),
             &quotient, &remainder);
  if ((Int128High64(lhs) < 0) != (Int128High64(rhs) < 0)) quotient = -quotient;
  return MakeInt128(int128_t_internal::BitCastToSigned(Uint128High64(quotient)),
                    Uint128Low64(quotient));
}

int128_t operator%(int128_t lhs, int128_t rhs) {
  assert(lhs != Int128Min() || rhs != -1);  // UB on two's complement.

  uint128_t quotient = 0;
  uint128_t remainder = 0;
  uint128_t::DivMod(UnsignedAbsoluteValue(lhs), UnsignedAbsoluteValue(rhs),
             &quotient, &remainder);
  if (Int128High64(lhs) < 0) remainder = -remainder;
  return MakeInt128(int128_t_internal::BitCastToSigned(Uint128High64(remainder)),
                    Uint128Low64(remainder));
}
#endif  // ABSL_HAVE_INTRINSIC_INT128

std::string int128_t::ToFormattedString(int128_t v, std::ios_base::fmtflags flags) {
  std::string rep;

  // Add the sign if needed.
  bool print_as_decimal =
    (flags & std::ios::basefield) == std::ios::dec ||
    (flags & std::ios::basefield) == std::ios_base::fmtflags();
  if (print_as_decimal) {
    if (Int128High64(v) < 0) {
      rep = "-";
    } else if (flags & std::ios::showpos) {
      rep = "+";
    }
  }

  rep.append(uint128_t::ToFormattedString(
      print_as_decimal ? UnsignedAbsoluteValue(v) : uint128_t(v), flags));
  return rep;
}

std::string int128_t::ToString(int128_t v) {
  return int128_t::ToFormattedString(v);
}

std::ostream& operator<<(std::ostream& os, int128_t v) {
  std::ios_base::fmtflags flags = os.flags();
  std::string rep = int128_t::ToFormattedString(v, flags);

  // Add the requisite padding.
  std::streamsize width = os.width(0);
  if (static_cast<size_t>(width) > rep.size()) {
    bool print_as_decimal =
      (flags & std::ios::basefield) == std::ios::dec ||
      (flags & std::ios::basefield) == std::ios_base::fmtflags();

    switch (flags & std::ios::adjustfield) {
      case std::ios::left:
        rep.append(width - rep.size(), os.fill());
        break;
      case std::ios::internal:
        if (print_as_decimal && (rep[0] == '+' || rep[0] == '-')) {
          rep.insert(1, width - rep.size(), os.fill());
        } else if ((flags & std::ios::basefield) == std::ios::hex &&
                   (flags & std::ios::showbase) && v != 0) {
          rep.insert(2, width - rep.size(), os.fill());
        } else {
          rep.insert(0, width - rep.size(), os.fill());
        }
        break;
      default:  // std::ios::right
        rep.insert(0, width - rep.size(), os.fill());
        break;
    }
  }

  return os << rep;
}

}  // namespace absl

namespace std {
constexpr bool numeric_limits<absl::uint128_t>::is_specialized;
constexpr bool numeric_limits<absl::uint128_t>::is_signed;
constexpr bool numeric_limits<absl::uint128_t>::is_integer;
constexpr bool numeric_limits<absl::uint128_t>::is_exact;
constexpr bool numeric_limits<absl::uint128_t>::has_infinity;
constexpr bool numeric_limits<absl::uint128_t>::has_quiet_NaN;
constexpr bool numeric_limits<absl::uint128_t>::has_signaling_NaN;
constexpr float_denorm_style numeric_limits<absl::uint128_t>::has_denorm;
constexpr bool numeric_limits<absl::uint128_t>::has_denorm_loss;
constexpr float_round_style numeric_limits<absl::uint128_t>::round_style;
constexpr bool numeric_limits<absl::uint128_t>::is_iec559;
constexpr bool numeric_limits<absl::uint128_t>::is_bounded;
constexpr bool numeric_limits<absl::uint128_t>::is_modulo;
constexpr int numeric_limits<absl::uint128_t>::digits;
constexpr int numeric_limits<absl::uint128_t>::digits10;
constexpr int numeric_limits<absl::uint128_t>::max_digits10;
constexpr int numeric_limits<absl::uint128_t>::radix;
constexpr int numeric_limits<absl::uint128_t>::min_exponent;
constexpr int numeric_limits<absl::uint128_t>::min_exponent10;
constexpr int numeric_limits<absl::uint128_t>::max_exponent;
constexpr int numeric_limits<absl::uint128_t>::max_exponent10;
constexpr bool numeric_limits<absl::uint128_t>::traps;
constexpr bool numeric_limits<absl::uint128_t>::tinyness_before;

constexpr bool numeric_limits<absl::int128_t>::is_specialized;
constexpr bool numeric_limits<absl::int128_t>::is_signed;
constexpr bool numeric_limits<absl::int128_t>::is_integer;
constexpr bool numeric_limits<absl::int128_t>::is_exact;
constexpr bool numeric_limits<absl::int128_t>::has_infinity;
constexpr bool numeric_limits<absl::int128_t>::has_quiet_NaN;
constexpr bool numeric_limits<absl::int128_t>::has_signaling_NaN;
constexpr float_denorm_style numeric_limits<absl::int128_t>::has_denorm;
constexpr bool numeric_limits<absl::int128_t>::has_denorm_loss;
constexpr float_round_style numeric_limits<absl::int128_t>::round_style;
constexpr bool numeric_limits<absl::int128_t>::is_iec559;
constexpr bool numeric_limits<absl::int128_t>::is_bounded;
constexpr bool numeric_limits<absl::int128_t>::is_modulo;
constexpr int numeric_limits<absl::int128_t>::digits;
constexpr int numeric_limits<absl::int128_t>::digits10;
constexpr int numeric_limits<absl::int128_t>::max_digits10;
constexpr int numeric_limits<absl::int128_t>::radix;
constexpr int numeric_limits<absl::int128_t>::min_exponent;
constexpr int numeric_limits<absl::int128_t>::min_exponent10;
constexpr int numeric_limits<absl::int128_t>::max_exponent;
constexpr int numeric_limits<absl::int128_t>::max_exponent10;
constexpr bool numeric_limits<absl::int128_t>::traps;
constexpr bool numeric_limits<absl::int128_t>::tinyness_before;
}  // namespace std
