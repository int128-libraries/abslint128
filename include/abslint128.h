//
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
//
// -----------------------------------------------------------------------------
// File: int128_t.h
// -----------------------------------------------------------------------------
//
// This header file defines 128-bit integer types, `uint128_t` and `int128_t`.

#ifndef ABSL_INT128_H_
#define ABSL_INT128_H_

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

#if defined(_MSC_VER)
// In very old versions of MSVC and when the /Zc:wchar_t flag is off, wchar_t is
// a typedef for unsigned short.  Otherwise wchar_t is mapped to the __wchar_t
// builtin type.  We need to make sure not to define operator wchar_t()
// alongside operator unsigned short() in these instances.
#define ABSL_INTERNAL_WCHAR_T __wchar_t
#if defined(_M_X64)
#include <intrin.h>
#pragma intrinsic(_umul128)
#endif  // defined(_M_X64)
#else   // defined(_MSC_VER)
#define ABSL_INTERNAL_WCHAR_T wchar_t
#endif  // defined(_MSC_VER)

#ifdef _WIN32
#ifdef abslint128_t_EXPORTS
#define ABSL_DLL __declspec(dllexport)
#else
#define ABSL_DLL __declspec(dllimport)
#endif
#else // _WIN32
#define ABSL_DLL
#endif // _WIN32

// ABSL_ATTRIBUTE_ALWAYS_INLINE
// ABSL_ATTRIBUTE_NOINLINE
//
// Forces functions to either inline or not inline. Introduced in gcc 3.1.
#if defined(__GNUC__) || defined(__clang__)
#define ABSL_ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !__INTEL_COMPILER && _MSC_VER >= 1310 // since Visual Studio .NET 2003
#define ABSL_ATTRIBUTE_ALWAYS_INLINE inline __forceinline
#else
#define ABSL_ATTRIBUTE_ALWAYS_INLINE inline
#endif

// ABSL_INTERNAL_ASSUME(cond)
// Informs the compiler than a condition is always true and that it can assume
// it to be true for optimization purposes. The call has undefined behavior if
// the condition is false.
// In !NDEBUG mode, the condition is checked with an assert().
// NOTE: The expression must not have side effects, as it will only be evaluated
// in some compilation modes and not others.
//
// Example:
//
//   int x = ...;
//   ABSL_INTERNAL_ASSUME(x >= 0);
//   // The compiler can optimize the division to a simple right shift using the
//   // assumption specified above.
//   int y = x / 16;
//
#if !defined(NDEBUG)
#define ABSL_INTERNAL_ASSUME(cond) assert(cond)
#elif ABSL_HAVE_BUILTIN(__builtin_assume)
#define ABSL_INTERNAL_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__) || ABSL_HAVE_BUILTIN(__builtin_unreachable)
#define ABSL_INTERNAL_ASSUME(cond)        \
  do {                                    \
    if (!(cond)) __builtin_unreachable(); \
  } while (0)
#elif defined(_MSC_VER)
#define ABSL_INTERNAL_ASSUME(cond) __assume(cond)
#else
#define ABSL_INTERNAL_ASSUME(cond)      \
  do {                                  \
    static_cast<void>(false && (cond)); \
  } while (0)
#endif

namespace absl {

class int128_t;

// uint128_t
//
// An unsigned 128-bit integer type. The API is meant to mimic an intrinsic type
// as closely as is practical, including exhibiting undefined behavior in
// analogous cases (e.g. division by zero). This type is intended to be a
// drop-in replacement once C++ supports an intrinsic `uint128_t_t` type; when
// that occurs, existing well-behaved uses of `uint128_t` will continue to work
// using that new type.
//
// Note: code written with this type will continue to compile once `uint128_t_t`
// is introduced, provided the replacement helper functions
// `Uint128(Low|High)64()` and `MakeUint128()` are made.
//
// A `uint128_t` supports the following:
//
//   * Implicit construction from integral types
//   * Explicit conversion to integral types
//
// Additionally, if your compiler supports `__int128_t`, `uint128_t` is
// interoperable with that type. (Abseil checks for this compatibility through
// the `ABSL_HAVE_INTRINSIC_INT128` macro.)
//
// However, a `uint128_t` differs from intrinsic integral types in the following
// ways:
//
//   * Errors on implicit conversions that do not preserve value (such as
//     loss of precision when converting to float values).
//   * Requires explicit construction from and conversion to floating point
//     types.
//   * Conversion to integral types requires an explicit static_cast() to
//     mimic use of the `-Wnarrowing` compiler flag.
//   * The alignment requirement of `uint128_t` may differ from that of an
//     intrinsic 128-bit integer type depending on platform and build
//     configuration.
//
// Example:
//
//     float y = absl::Uint128Max();  // Error. uint128_t cannot be implicitly
//                                    // converted to float.
//
//     absl::uint128_t v;
//     uint64_t i = v;                         // Error
//     uint64_t i = static_cast<uint64_t>(v);  // OK
//
class
#if defined(ABSL_HAVE_INTRINSIC_INT128)
    alignas(unsigned __int128_t)
#endif  // ABSL_HAVE_INTRINSIC_INT128
        uint128_t {
 public:
  uint128_t() = default;

  // Constructors from arithmetic types
  constexpr uint128_t(int v);                 // NOLINT(runtime/explicit)
  constexpr uint128_t(unsigned int v);        // NOLINT(runtime/explicit)
  constexpr uint128_t(long v);                // NOLINT(runtime/int)
  constexpr uint128_t(unsigned long v);       // NOLINT(runtime/int)
  constexpr uint128_t(long long v);           // NOLINT(runtime/int)
  constexpr uint128_t(unsigned long long v);  // NOLINT(runtime/int)
#ifdef ABSL_HAVE_INTRINSIC_INT128
  constexpr uint128_t(__int128_t v);           // NOLINT(runtime/explicit)
  constexpr uint128_t(unsigned __int128_t v);  // NOLINT(runtime/explicit)
#endif  // ABSL_HAVE_INTRINSIC_INT128
  constexpr uint128_t(int128_t v);  // NOLINT(runtime/explicit)
  explicit uint128_t(float v);
  explicit uint128_t(double v);
  explicit uint128_t(long double v);

  // Assignment operators from arithmetic types
  uint128_t& operator=(int v);
  uint128_t& operator=(unsigned int v);
  uint128_t& operator=(long v);                // NOLINT(runtime/int)
  uint128_t& operator=(unsigned long v);       // NOLINT(runtime/int)
  uint128_t& operator=(long long v);           // NOLINT(runtime/int)
  uint128_t& operator=(unsigned long long v);  // NOLINT(runtime/int)
#ifdef ABSL_HAVE_INTRINSIC_INT128
  uint128_t& operator=(__int128_t v);
  uint128_t& operator=(unsigned __int128_t v);
#endif  // ABSL_HAVE_INTRINSIC_INT128
  uint128_t& operator=(int128_t v);

  // Conversion operators to other arithmetic types
  constexpr explicit operator bool() const;
  constexpr explicit operator char() const;
  constexpr explicit operator signed char() const;
  constexpr explicit operator unsigned char() const;
  constexpr explicit operator char16_t() const;
  constexpr explicit operator char32_t() const;
  constexpr explicit operator ABSL_INTERNAL_WCHAR_T() const;
  constexpr explicit operator short() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned short() const;
  constexpr explicit operator int() const;
  constexpr explicit operator unsigned int() const;
  constexpr explicit operator long() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator long long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long long() const;
#ifdef ABSL_HAVE_INTRINSIC_INT128
  constexpr explicit operator __int128_t() const;
  constexpr explicit operator unsigned __int128_t() const;
#endif  // ABSL_HAVE_INTRINSIC_INT128
  explicit operator float() const;
  explicit operator double() const;
  explicit operator long double() const;

  // Trivial copy constructor, assignment operator and destructor.

  // Arithmetic operators.
  uint128_t& operator+=(uint128_t other);
  uint128_t& operator-=(uint128_t other);
  uint128_t& operator*=(uint128_t other);
  // Long division/modulo for uint128_t.
  uint128_t& operator/=(uint128_t other);
  uint128_t& operator%=(uint128_t other);
  uint128_t operator++(int);
  uint128_t operator--(int);
  uint128_t& operator<<=(int);
  uint128_t& operator>>=(int);
  uint128_t& operator&=(uint128_t other);
  uint128_t& operator|=(uint128_t other);
  uint128_t& operator^=(uint128_t other);
  uint128_t& operator++();
  uint128_t& operator--();

  // Uint128Low64()
  //
  // Returns the lower 64-bit value of a `uint128_t` value.
  friend constexpr uint64_t Uint128Low64(uint128_t v);

  // Uint128High64()
  //
  // Returns the higher 64-bit value of a `uint128_t` value.
  friend constexpr uint64_t Uint128High64(uint128_t v);

  // MakeUInt128()
  //
  // Constructs a `uint128_t` numeric value from two 64-bit unsigned integers.
  // Note that this factory function is the only way to construct a `uint128_t`
  // from integer values greater than 2^64.
  //
  // Example:
  //
  //   absl::uint128_t big = absl::MakeUint128(1, 0);
  friend constexpr uint128_t MakeUint128(uint64_t high, uint64_t low);

  // Uint128Max()
  //
  // Returns the highest value for a 128-bit unsigned integer.
  friend constexpr uint128_t Uint128Max();

  // Support for absl::Hash.
  template <typename H>
  friend H AbslHashValue(H h, uint128_t v) {
    return H::combine(std::move(h), Uint128High64(v), Uint128Low64(v));
  }

  // Combined division/modulo for a 128-bit unsigned integer.
  static void DivMod(uint128_t dividend, uint128_t divisor, uint128_t* quotient_ret,
                     uint128_t* remainder_ret);

  static std::string ToFormattedString(uint128_t v, std::ios_base::fmtflags flags = std::ios_base::fmtflags());

  static std::string ToString(uint128_t v);

 private:
  constexpr uint128_t(uint64_t high, uint64_t low);

  // TODO(strel) Update implementation to use __int128_t once all users of
  // uint128_t are fixed to not depend on alignof(uint128_t) == 8. Also add
  // alignas(16) to class definition to keep alignment consistent across
  // platforms.
#if defined(ABSL_IS_LITTLE_ENDIAN)
  uint64_t lo_;
  uint64_t hi_;
#elif defined(ABSL_IS_BIG_ENDIAN)
  uint64_t hi_;
  uint64_t lo_;
#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order
};

// allow uint128_t to be logged
std::ostream& operator<<(std::ostream& os, uint128_t v);

// TODO(strel) add operator>>(std::istream&, uint128_t)

constexpr uint128_t Uint128Max() {
  return uint128_t((std::numeric_limits<uint64_t>::max)(),
                 (std::numeric_limits<uint64_t>::max)());
}

}  // namespace absl

// Specialized numeric_limits for uint128_t.
namespace std {
template <>
class numeric_limits<absl::uint128_t> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = false;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr float_denorm_style has_denorm = denorm_absent;
  static constexpr bool has_denorm_loss = false;
  static constexpr float_round_style round_style = round_toward_zero;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = true;
  static constexpr int digits = 128;
  static constexpr int digits10 = 38;
  static constexpr int max_digits10 = 0;
  static constexpr int radix = 2;
  static constexpr int min_exponent = 0;
  static constexpr int min_exponent10 = 0;
  static constexpr int max_exponent = 0;
  static constexpr int max_exponent10 = 0;
#ifdef ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool traps = numeric_limits<unsigned __int128_t>::traps;
#else   // ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool traps = numeric_limits<uint64_t>::traps;
#endif  // ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool tinyness_before = false;

  static constexpr absl::uint128_t (min)() { return 0; }
  static constexpr absl::uint128_t lowest() { return 0; }
  static constexpr absl::uint128_t (max)() { return absl::Uint128Max(); }
  static constexpr absl::uint128_t epsilon() { return 0; }
  static constexpr absl::uint128_t round_error() { return 0; }
  static constexpr absl::uint128_t infinity() { return 0; }
  static constexpr absl::uint128_t quiet_NaN() { return 0; }
  static constexpr absl::uint128_t signaling_NaN() { return 0; }
  static constexpr absl::uint128_t denorm_min() { return 0; }
};
}  // namespace std

namespace absl {

// int128_t
//
// A signed 128-bit integer type. The API is meant to mimic an intrinsic
// integral type as closely as is practical, including exhibiting undefined
// behavior in analogous cases (e.g. division by zero).
//
// An `int128_t` supports the following:
//
//   * Implicit construction from integral types
//   * Explicit conversion to integral types
//
// However, an `int128_t` differs from intrinsic integral types in the following
// ways:
//
//   * It is not implicitly convertible to other integral types.
//   * Requires explicit construction from and conversion to floating point
//     types.

// Additionally, if your compiler supports `__int128_t`, `int128_t` is
// interoperable with that type. (Abseil checks for this compatibility through
// the `ABSL_HAVE_INTRINSIC_INT128` macro.)
//
// The design goal for `int128_t` is that it will be compatible with a future
// `int128_t_t`, if that type becomes a part of the standard.
//
// Example:
//
//     float y = absl::int128_t(17);  // Error. int128_t cannot be implicitly
//                                  // converted to float.
//
//     absl::int128_t v;
//     int64_t i = v;                        // Error
//     int64_t i = static_cast<int64_t>(v);  // OK
//
class int128_t {
 public:
  int128_t() = default;

  // Constructors from arithmetic types
  constexpr int128_t(int v);                 // NOLINT(runtime/explicit)
  constexpr int128_t(unsigned int v);        // NOLINT(runtime/explicit)
  constexpr int128_t(long v);                // NOLINT(runtime/int)
  constexpr int128_t(unsigned long v);       // NOLINT(runtime/int)
  constexpr int128_t(long long v);           // NOLINT(runtime/int)
  constexpr int128_t(unsigned long long v);  // NOLINT(runtime/int)
#ifdef ABSL_HAVE_INTRINSIC_INT128
  constexpr int128_t(__int128_t v);  // NOLINT(runtime/explicit)
  constexpr explicit int128_t(unsigned __int128_t v);
#endif  // ABSL_HAVE_INTRINSIC_INT128
  constexpr explicit int128_t(uint128_t v);
  explicit int128_t(float v);
  explicit int128_t(double v);
  explicit int128_t(long double v);

  // Assignment operators from arithmetic types
  int128_t& operator=(int v);
  int128_t& operator=(unsigned int v);
  int128_t& operator=(long v);                // NOLINT(runtime/int)
  int128_t& operator=(unsigned long v);       // NOLINT(runtime/int)
  int128_t& operator=(long long v);           // NOLINT(runtime/int)
  int128_t& operator=(unsigned long long v);  // NOLINT(runtime/int)
#ifdef ABSL_HAVE_INTRINSIC_INT128
  int128_t& operator=(__int128_t v);
#endif  // ABSL_HAVE_INTRINSIC_INT128

  // Conversion operators to other arithmetic types
  constexpr explicit operator bool() const;
  constexpr explicit operator char() const;
  constexpr explicit operator signed char() const;
  constexpr explicit operator unsigned char() const;
  constexpr explicit operator char16_t() const;
  constexpr explicit operator char32_t() const;
  constexpr explicit operator ABSL_INTERNAL_WCHAR_T() const;
  constexpr explicit operator short() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned short() const;
  constexpr explicit operator int() const;
  constexpr explicit operator unsigned int() const;
  constexpr explicit operator long() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator long long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long long() const;
#ifdef ABSL_HAVE_INTRINSIC_INT128
  constexpr explicit operator __int128_t() const;
  constexpr explicit operator unsigned __int128_t() const;
#endif  // ABSL_HAVE_INTRINSIC_INT128
  explicit operator float() const;
  explicit operator double() const;
  explicit operator long double() const;

  // Trivial copy constructor, assignment operator and destructor.

  // Arithmetic operators
  int128_t& operator+=(int128_t other);
  int128_t& operator-=(int128_t other);
  int128_t& operator*=(int128_t other);
  int128_t& operator/=(int128_t other);
  int128_t& operator%=(int128_t other);
  int128_t operator++(int);  // postfix increment: i++
  int128_t operator--(int);  // postfix decrement: i--
  int128_t& operator++();    // prefix increment:  ++i
  int128_t& operator--();    // prefix decrement:  --i
  int128_t& operator&=(int128_t other);
  int128_t& operator|=(int128_t other);
  int128_t& operator^=(int128_t other);
  int128_t& operator<<=(int amount);
  int128_t& operator>>=(int amount);

  // Int128Low64()
  //
  // Returns the lower 64-bit value of a `int128_t` value.
  friend constexpr uint64_t Int128Low64(int128_t v);

  // Int128High64()
  //
  // Returns the higher 64-bit value of a `int128_t` value.
  friend constexpr int64_t Int128High64(int128_t v);

  // MakeInt128()
  //
  // Constructs a `int128_t` numeric value from two 64-bit integers. Note that
  // signedness is conveyed in the upper `high` value.
  //
  //   (absl::int128_t(1) << 64) * high + low
  //
  // Note that this factory function is the only way to construct a `int128_t`
  // from integer values greater than 2^64 or less than -2^64.
  //
  // Example:
  //
  //   absl::int128_t big = absl::MakeInt128(1, 0);
  //   absl::int128_t big_n = absl::MakeInt128(-1, 0);
  friend constexpr int128_t MakeInt128(int64_t high, uint64_t low);

  // Int128Max()
  //
  // Returns the maximum value for a 128-bit signed integer.
  friend constexpr int128_t Int128Max();

  // Int128Min()
  //
  // Returns the minimum value for a 128-bit signed integer.
  friend constexpr int128_t Int128Min();

  // Support for absl::Hash.
  template <typename H>
  friend H AbslHashValue(H h, int128_t v) {
    return H::combine(std::move(h), Int128High64(v), Int128Low64(v));
  }

  // Combined division/modulo for a 128-bit signed integer.
  static void DivMod(int128_t dividend, int128_t divisor, int128_t* quotient_ret,
                     int128_t* remainder_ret);

  static std::string ToFormattedString(int128_t v, std::ios_base::fmtflags flags = std::ios_base::fmtflags());

  static std::string ToString(int128_t v);

 private:
  constexpr int128_t(int64_t high, uint64_t low);

#if defined(ABSL_HAVE_INTRINSIC_INT128)
  __int128_t v_;
#else  // ABSL_HAVE_INTRINSIC_INT128
#if defined(ABSL_IS_LITTLE_ENDIAN)
  uint64_t lo_;
  int64_t hi_;
#elif defined(ABSL_IS_BIG_ENDIAN)
  int64_t hi_;
  uint64_t lo_;
#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order
#endif  // ABSL_HAVE_INTRINSIC_INT128
};

std::ostream& operator<<(std::ostream& os, int128_t v);

// TODO(absl-team) add operator>>(std::istream&, int128_t)

constexpr int128_t Int128Max() {
  return int128_t((std::numeric_limits<int64_t>::max)(),
                (std::numeric_limits<uint64_t>::max)());
}

constexpr int128_t Int128Min() {
  return int128_t((std::numeric_limits<int64_t>::min)(), 0);
}

}  // namespace absl

// Specialized numeric_limits for int128_t.
namespace std {
template <>
class numeric_limits<absl::int128_t> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = true;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr float_denorm_style has_denorm = denorm_absent;
  static constexpr bool has_denorm_loss = false;
  static constexpr float_round_style round_style = round_toward_zero;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = false;
  static constexpr int digits = 127;
  static constexpr int digits10 = 38;
  static constexpr int max_digits10 = 0;
  static constexpr int radix = 2;
  static constexpr int min_exponent = 0;
  static constexpr int min_exponent10 = 0;
  static constexpr int max_exponent = 0;
  static constexpr int max_exponent10 = 0;
#ifdef ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool traps = numeric_limits<__int128_t>::traps;
#else   // ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool traps = numeric_limits<uint64_t>::traps;
#endif  // ABSL_HAVE_INTRINSIC_INT128
  static constexpr bool tinyness_before = false;

  static constexpr absl::int128_t (min)() { return absl::Int128Min(); }
  static constexpr absl::int128_t lowest() { return absl::Int128Min(); }
  static constexpr absl::int128_t (max)() { return absl::Int128Max(); }
  static constexpr absl::int128_t epsilon() { return 0; }
  static constexpr absl::int128_t round_error() { return 0; }
  static constexpr absl::int128_t infinity() { return 0; }
  static constexpr absl::int128_t quiet_NaN() { return 0; }
  static constexpr absl::int128_t signaling_NaN() { return 0; }
  static constexpr absl::int128_t denorm_min() { return 0; }
};
}  // namespace std

// --------------------------------------------------------------------------
//                      Implementation details follow
// --------------------------------------------------------------------------
namespace absl {

constexpr uint128_t MakeUint128(uint64_t high, uint64_t low) {
  return uint128_t(high, low);
}

// Assignment from integer types.

inline uint128_t& uint128_t::operator=(int v) { return *this = uint128_t(v); }

inline uint128_t& uint128_t::operator=(unsigned int v) {
  return *this = uint128_t(v);
}

inline uint128_t& uint128_t::operator=(long v) {  // NOLINT(runtime/int)
  return *this = uint128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline uint128_t& uint128_t::operator=(unsigned long v) {
  return *this = uint128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline uint128_t& uint128_t::operator=(long long v) {
  return *this = uint128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline uint128_t& uint128_t::operator=(unsigned long long v) {
  return *this = uint128_t(v);
}

#ifdef ABSL_HAVE_INTRINSIC_INT128
inline uint128_t& uint128_t::operator=(__int128_t v) {
  return *this = uint128_t(v);
}

inline uint128_t& uint128_t::operator=(unsigned __int128_t v) {
  return *this = uint128_t(v);
}
#endif  // ABSL_HAVE_INTRINSIC_INT128

inline uint128_t& uint128_t::operator=(int128_t v) {
  return *this = uint128_t(v);
}

// Arithmetic operators.

uint128_t operator<<(uint128_t lhs, int amount);
uint128_t operator>>(uint128_t lhs, int amount);
uint128_t operator+(uint128_t lhs, uint128_t rhs);
uint128_t operator-(uint128_t lhs, uint128_t rhs);
uint128_t operator*(uint128_t lhs, uint128_t rhs);
uint128_t operator/(uint128_t lhs, uint128_t rhs);
uint128_t operator%(uint128_t lhs, uint128_t rhs);

inline uint128_t& uint128_t::operator<<=(int amount) {
  *this = *this << amount;
  return *this;
}

inline uint128_t& uint128_t::operator>>=(int amount) {
  *this = *this >> amount;
  return *this;
}

inline uint128_t& uint128_t::operator+=(uint128_t other) {
  *this = *this + other;
  return *this;
}

inline uint128_t& uint128_t::operator-=(uint128_t other) {
  *this = *this - other;
  return *this;
}

inline uint128_t& uint128_t::operator*=(uint128_t other) {
  *this = *this * other;
  return *this;
}

inline uint128_t& uint128_t::operator/=(uint128_t other) {
  *this = *this / other;
  return *this;
}

inline uint128_t& uint128_t::operator%=(uint128_t other) {
  *this = *this % other;
  return *this;
}

constexpr uint64_t Uint128Low64(uint128_t v) { return v.lo_; }

constexpr uint64_t Uint128High64(uint128_t v) { return v.hi_; }

// Constructors from integer types.

#if defined(ABSL_IS_LITTLE_ENDIAN)

constexpr uint128_t::uint128_t(uint64_t high, uint64_t low)
    : lo_{low}, hi_{high} {}

constexpr uint128_t::uint128_t(int v)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}
constexpr uint128_t::uint128_t(long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}
constexpr uint128_t::uint128_t(long long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}

constexpr uint128_t::uint128_t(unsigned int v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::uint128_t(unsigned long v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::uint128_t(unsigned long long v) : lo_{v}, hi_{0} {}

#ifdef ABSL_HAVE_INTRINSIC_INT128
constexpr uint128_t::uint128_t(__int128_t v)
    : lo_{static_cast<uint64_t>(v & ~uint64_t{0})},
      hi_{static_cast<uint64_t>(static_cast<unsigned __int128_t>(v) >> 64)} {}
constexpr uint128_t::uint128_t(unsigned __int128_t v)
    : lo_{static_cast<uint64_t>(v & ~uint64_t{0})},
      hi_{static_cast<uint64_t>(v >> 64)} {}
#endif  // ABSL_HAVE_INTRINSIC_INT128

constexpr uint128_t::uint128_t(int128_t v)
    : lo_{Int128Low64(v)}, hi_{static_cast<uint64_t>(Int128High64(v))} {}

#elif defined(ABSL_IS_BIG_ENDIAN)

constexpr uint128_t::uint128_t(uint64_t high, uint64_t low)
    : hi_{high}, lo_{low} {}

constexpr uint128_t::uint128_t(int v)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}
constexpr uint128_t::uint128_t(long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}
constexpr uint128_t::uint128_t(long long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}

constexpr uint128_t::uint128_t(unsigned int v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::uint128_t(unsigned long v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::uint128_t(unsigned long long v) : hi_{0}, lo_{v} {}

#ifdef ABSL_HAVE_INTRINSIC_INT128
constexpr uint128_t::uint128_t(__int128_t v)
    : hi_{static_cast<uint64_t>(static_cast<unsigned __int128_t>(v) >> 64)},
      lo_{static_cast<uint64_t>(v & ~uint64_t{0})} {}
constexpr uint128_t::uint128_t(unsigned __int128_t v)
    : hi_{static_cast<uint64_t>(v >> 64)},
      lo_{static_cast<uint64_t>(v & ~uint64_t{0})} {}
#endif  // ABSL_HAVE_INTRINSIC_INT128

constexpr uint128_t::uint128_t(int128_t v)
    : hi_{static_cast<uint64_t>(Int128High64(v))}, lo_{Int128Low64(v)} {}

#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order

// Conversion operators to integer types.

constexpr uint128_t::operator bool() const { return lo_ || hi_; }

constexpr uint128_t::operator char() const { return static_cast<char>(lo_); }

constexpr uint128_t::operator signed char() const {
  return static_cast<signed char>(lo_);
}

constexpr uint128_t::operator unsigned char() const {
  return static_cast<unsigned char>(lo_);
}

constexpr uint128_t::operator char16_t() const {
  return static_cast<char16_t>(lo_);
}

constexpr uint128_t::operator char32_t() const {
  return static_cast<char32_t>(lo_);
}

constexpr uint128_t::operator ABSL_INTERNAL_WCHAR_T() const {
  return static_cast<ABSL_INTERNAL_WCHAR_T>(lo_);
}

// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::operator short() const { return static_cast<short>(lo_); }

constexpr uint128_t::operator unsigned short() const {  // NOLINT(runtime/int)
  return static_cast<unsigned short>(lo_);            // NOLINT(runtime/int)
}

constexpr uint128_t::operator int() const { return static_cast<int>(lo_); }

constexpr uint128_t::operator unsigned int() const {
  return static_cast<unsigned int>(lo_);
}

// NOLINTNEXTLINE(runtime/int)
constexpr uint128_t::operator long() const { return static_cast<long>(lo_); }

constexpr uint128_t::operator unsigned long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long>(lo_);            // NOLINT(runtime/int)
}

constexpr uint128_t::operator long long() const {  // NOLINT(runtime/int)
  return static_cast<long long>(lo_);            // NOLINT(runtime/int)
}

constexpr uint128_t::operator unsigned long long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long long>(lo_);            // NOLINT(runtime/int)
}

#ifdef ABSL_HAVE_INTRINSIC_INT128
constexpr uint128_t::operator __int128_t() const {
  return (static_cast<__int128_t>(hi_) << 64) + lo_;
}

constexpr uint128_t::operator unsigned __int128_t() const {
  return (static_cast<unsigned __int128_t>(hi_) << 64) + lo_;
}
#endif  // ABSL_HAVE_INTRINSIC_INT128

// Conversion operators to floating point types.

inline uint128_t::operator float() const {
  return static_cast<float>(lo_) + std::ldexp(static_cast<float>(hi_), 64);
}

inline uint128_t::operator double() const {
  return static_cast<double>(lo_) + std::ldexp(static_cast<double>(hi_), 64);
}

inline uint128_t::operator long double() const {
  return static_cast<long double>(lo_) +
         std::ldexp(static_cast<long double>(hi_), 64);
}

// Comparison operators.

inline bool operator==(uint128_t lhs, uint128_t rhs) {
  return (Uint128Low64(lhs) == Uint128Low64(rhs) &&
          Uint128High64(lhs) == Uint128High64(rhs));
}

inline bool operator!=(uint128_t lhs, uint128_t rhs) {
  return !(lhs == rhs);
}

inline bool operator<(uint128_t lhs, uint128_t rhs) {
#ifdef ABSL_HAVE_INTRINSIC_INT128
  return static_cast<unsigned __int128_t>(lhs) <
         static_cast<unsigned __int128_t>(rhs);
#else
  return (Uint128High64(lhs) == Uint128High64(rhs))
             ? (Uint128Low64(lhs) < Uint128Low64(rhs))
             : (Uint128High64(lhs) < Uint128High64(rhs));
#endif
}

inline bool operator>(uint128_t lhs, uint128_t rhs) { return rhs < lhs; }

inline bool operator<=(uint128_t lhs, uint128_t rhs) { return !(rhs < lhs); }

inline bool operator>=(uint128_t lhs, uint128_t rhs) { return !(lhs < rhs); }

// Unary operators.

inline uint128_t operator-(uint128_t val) {
  uint64_t hi = ~Uint128High64(val);
  uint64_t lo = ~Uint128Low64(val) + 1;
  if (lo == 0) ++hi;  // carry
  return MakeUint128(hi, lo);
}

inline bool operator!(uint128_t val) {
  return !Uint128High64(val) && !Uint128Low64(val);
}

// Logical operators.

inline uint128_t operator~(uint128_t val) {
  return MakeUint128(~Uint128High64(val), ~Uint128Low64(val));
}

inline uint128_t operator|(uint128_t lhs, uint128_t rhs) {
  return MakeUint128(Uint128High64(lhs) | Uint128High64(rhs),
                           Uint128Low64(lhs) | Uint128Low64(rhs));
}

inline uint128_t operator&(uint128_t lhs, uint128_t rhs) {
  return MakeUint128(Uint128High64(lhs) & Uint128High64(rhs),
                           Uint128Low64(lhs) & Uint128Low64(rhs));
}

inline uint128_t operator^(uint128_t lhs, uint128_t rhs) {
  return MakeUint128(Uint128High64(lhs) ^ Uint128High64(rhs),
                           Uint128Low64(lhs) ^ Uint128Low64(rhs));
}

inline uint128_t& uint128_t::operator|=(uint128_t other) {
  hi_ |= other.hi_;
  lo_ |= other.lo_;
  return *this;
}

inline uint128_t& uint128_t::operator&=(uint128_t other) {
  hi_ &= other.hi_;
  lo_ &= other.lo_;
  return *this;
}

inline uint128_t& uint128_t::operator^=(uint128_t other) {
  hi_ ^= other.hi_;
  lo_ ^= other.lo_;
  return *this;
}

// Arithmetic operators.

inline uint128_t operator<<(uint128_t lhs, int amount) {
#ifdef ABSL_HAVE_INTRINSIC_INT128
  return static_cast<unsigned __int128_t>(lhs) << amount;
#else
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount != 0) {
      return MakeUint128(
          (Uint128High64(lhs) << amount) | (Uint128Low64(lhs) >> (64 - amount)),
          Uint128Low64(lhs) << amount);
    }
    return lhs;
  }
  return MakeUint128(Uint128Low64(lhs) << (amount - 64), 0);
#endif
}

inline uint128_t operator>>(uint128_t lhs, int amount) {
#ifdef ABSL_HAVE_INTRINSIC_INT128
  return static_cast<unsigned __int128_t>(lhs) >> amount;
#else
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  if (amount < 64) {
    if (amount != 0) {
      return MakeUint128(Uint128High64(lhs) >> amount,
                         (Uint128Low64(lhs) >> amount) |
                             (Uint128High64(lhs) << (64 - amount)));
    }
    return lhs;
  }
  return MakeUint128(0, Uint128High64(lhs) >> (amount - 64));
#endif
}

inline uint128_t operator+(uint128_t lhs, uint128_t rhs) {
  uint128_t result = MakeUint128(Uint128High64(lhs) + Uint128High64(rhs),
                               Uint128Low64(lhs) + Uint128Low64(rhs));
  if (Uint128Low64(result) < Uint128Low64(lhs)) {  // check for carry
    return MakeUint128(Uint128High64(result) + 1, Uint128Low64(result));
  }
  return result;
}

inline uint128_t operator-(uint128_t lhs, uint128_t rhs) {
  uint128_t result = MakeUint128(Uint128High64(lhs) - Uint128High64(rhs),
                               Uint128Low64(lhs) - Uint128Low64(rhs));
  if (Uint128Low64(lhs) < Uint128Low64(rhs)) {  // check for carry
    return MakeUint128(Uint128High64(result) - 1, Uint128Low64(result));
  }
  return result;
}

inline uint128_t operator*(uint128_t lhs, uint128_t rhs) {
#if defined(ABSL_HAVE_INTRINSIC_INT128)
  // TODO(strel) Remove once alignment issues are resolved and unsigned __int128_t
  // can be used for uint128_t storage.
  return static_cast<unsigned __int128_t>(lhs) *
         static_cast<unsigned __int128_t>(rhs);
#elif defined(_MSC_VER) && defined(_M_X64)
  uint64_t carry;
  uint64_t low = _umul128(Uint128Low64(lhs), Uint128Low64(rhs), &carry);
  return MakeUint128(Uint128Low64(lhs) * Uint128High64(rhs) +
                         Uint128High64(lhs) * Uint128Low64(rhs) + carry,
                     low);
#else   // ABSL_HAVE_INTRINSIC128
  uint64_t a32 = Uint128Low64(lhs) >> 32;
  uint64_t a00 = Uint128Low64(lhs) & 0xffffffff;
  uint64_t b32 = Uint128Low64(rhs) >> 32;
  uint64_t b00 = Uint128Low64(rhs) & 0xffffffff;
  uint128_t result =
      MakeUint128(Uint128High64(lhs) * Uint128Low64(rhs) +
                      Uint128Low64(lhs) * Uint128High64(rhs) + a32 * b32,
                  a00 * b00);
  result += uint128_t(a32 * b00) << 32;
  result += uint128_t(a00 * b32) << 32;
  return result;
#endif  // ABSL_HAVE_INTRINSIC128
}

// Increment/decrement operators.

inline uint128_t uint128_t::operator++(int) {
  uint128_t tmp(*this);
  *this += 1;
  return tmp;
}

inline uint128_t uint128_t::operator--(int) {
  uint128_t tmp(*this);
  *this -= 1;
  return tmp;
}

inline uint128_t& uint128_t::operator++() {
  *this += 1;
  return *this;
}

inline uint128_t& uint128_t::operator--() {
  *this -= 1;
  return *this;
}

constexpr int128_t MakeInt128(int64_t high, uint64_t low) {
  return int128_t(high, low);
}

// Assignment from integer types.
inline int128_t& int128_t::operator=(int v) {
  return *this = int128_t(v);
}

inline int128_t& int128_t::operator=(unsigned int v) {
  return *this = int128_t(v);
}

inline int128_t& int128_t::operator=(long v) {  // NOLINT(runtime/int)
  return *this = int128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline int128_t& int128_t::operator=(unsigned long v) {
  return *this = int128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline int128_t& int128_t::operator=(long long v) {
  return *this = int128_t(v);
}

// NOLINTNEXTLINE(runtime/int)
inline int128_t& int128_t::operator=(unsigned long long v) {
  return *this = int128_t(v);
}

// Arithmetic operators.

int128_t operator+(int128_t lhs, int128_t rhs);
int128_t operator-(int128_t lhs, int128_t rhs);
int128_t operator*(int128_t lhs, int128_t rhs);
int128_t operator/(int128_t lhs, int128_t rhs);
int128_t operator%(int128_t lhs, int128_t rhs);
int128_t operator|(int128_t lhs, int128_t rhs);
int128_t operator&(int128_t lhs, int128_t rhs);
int128_t operator^(int128_t lhs, int128_t rhs);
int128_t operator<<(int128_t lhs, int amount);
int128_t operator>>(int128_t lhs, int amount);

inline int128_t& int128_t::operator+=(int128_t other) {
  *this = *this + other;
  return *this;
}

inline int128_t& int128_t::operator-=(int128_t other) {
  *this = *this - other;
  return *this;
}

inline int128_t& int128_t::operator*=(int128_t other) {
  *this = *this * other;
  return *this;
}

inline int128_t& int128_t::operator/=(int128_t other) {
  *this = *this / other;
  return *this;
}

inline int128_t& int128_t::operator%=(int128_t other) {
  *this = *this % other;
  return *this;
}

inline int128_t& int128_t::operator|=(int128_t other) {
  *this = *this | other;
  return *this;
}

inline int128_t& int128_t::operator&=(int128_t other) {
  *this = *this & other;
  return *this;
}

inline int128_t& int128_t::operator^=(int128_t other) {
  *this = *this ^ other;
  return *this;
}

inline int128_t& int128_t::operator<<=(int amount) {
  *this = *this << amount;
  return *this;
}

inline int128_t& int128_t::operator>>=(int amount) {
  *this = *this >> amount;
  return *this;
}

namespace int128_t_internal {

// Casts from unsigned to signed while preserving the underlying binary
// representation.
constexpr int64_t BitCastToSigned(uint64_t v) {
  // Casting an unsigned integer to a signed integer of the same
  // width is implementation defined behavior if the source value would not fit
  // in the destination type. We step around it with a roundtrip bitwise not
  // operation to make sure this function remains constexpr. Clang, GCC, and
  // MSVC optimize this to a no-op on x86-64.
  return v & (uint64_t{1} << 63) ? ~static_cast<int64_t>(~v)
                                 : static_cast<int64_t>(v);
}

}  // namespace int128_t_internal

#if defined(ABSL_HAVE_INTRINSIC_INT128)
#include "int128_have_intrinsic.inc"  // IWYU pragma: export
#else  // ABSL_HAVE_INTRINSIC_INT128
#include "int128_no_intrinsic.inc"  // IWYU pragma: export
#endif  // ABSL_HAVE_INTRINSIC_INT128

}  // namespace absl

#undef ABSL_INTERNAL_WCHAR_T

#endif  // ABSL_INT128_H_
