#ifndef CPP_HELPERS_H
#define CPP_HELPERS_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <sstream>

template <typename ... Args>
std::string make_string(Args&& ... args)
{
    std::ostringstream ss;
    (void)std::initializer_list<bool> {(ss << args, false)...};
    return ss.str();
}

template <typename T, typename ... Args>
T make_error(Args&& ... args)
{
    return T(make_string(std::forward<Args>(args)...));
}



template<typename TargetType, bool Throw, typename ValueType,
         std::enable_if_t<Throw == false> * = nullptr>
constexpr bool is_value_in_type_range(ValueType x)
{
    return !(x > std::numeric_limits<TargetType>::max() || x < std::numeric_limits<TargetType>::min());
}

template<typename TargetType, bool Throw, typename ValueType,
         std::enable_if_t<Throw == true> * = nullptr>
constexpr void is_value_in_type_range(ValueType x)
{
    if (!is_value_in_type_range<TargetType, false>(x))
        throw make_error<std::runtime_error>("value ", x, " is out of range");
}

// Required separately because of the long::max to double conversion warning.
template<>
constexpr bool is_value_in_type_range<long, false, double>(double x)
{   // Checking x >= max because long::max (9223372036854775807) becomes (9223372036854775808).
    return !(x < std::numeric_limits<long>::min() || x >= static_cast<double>(std::numeric_limits<long>::max()));
}



template<typename T>
typename std::enable_if_t<std::is_floating_point<T>::value, bool>
constexpr approximately_equal(T x, T y)
{
    const auto absDiff = std::fabs(x - y);
    return absDiff <= std::numeric_limits<T>::epsilon() * std::max(std::fabs(x), std::fabs(y))
            || absDiff < std::numeric_limits<T>::min();
}

// https://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
// QUESTION Will (+x, -x) return always false (except subnormal case) and is it the expected result?
template<typename T>
typename std::enable_if_t<!std::numeric_limits<T>::is_integer, bool>
constexpr almost_equal(T x, T y, int ulp)
{
    const auto absDiff = std::fabs(x - y);
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return absDiff <= std::numeric_limits<T>::epsilon() * std::fabs(x + y) * ulp
        // unless the result is subnormal
        || absDiff < std::numeric_limits<T>::min();
}



template<typename T, typename U,
         std::enable_if_t<std::is_same<T, U>::value && std::is_arithmetic<T>::value && std::is_arithmetic<U>::value> * = nullptr>
inline T lossless_static_cast(U x)
{
    return x;
}

template<typename T, typename U,
         std::enable_if_t<!std::is_same<T, U>::value && std::is_arithmetic<T>::value && std::is_arithmetic<U>::value> * = nullptr>
T lossless_static_cast(U x);



template<>
constexpr int lossless_static_cast(unsigned char x)
{
    return static_cast<int>(x);
}

template<>
constexpr int lossless_static_cast(unsigned short x)
{
    return static_cast<int>(x);
}

template<>
inline int lossless_static_cast(uint32_t x)
{
    if (x > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<int>(x);
}

template<>
inline int lossless_static_cast(long x)
{
    is_value_in_type_range<int, true>(x);
    return static_cast<int>(x);
}

template<>
inline int lossless_static_cast(unsigned long x)
{
    if (x > static_cast<unsigned long>(std::numeric_limits<int>::max()))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<int>(x);
}

template<>
inline long lossless_static_cast(unsigned long x)
{
    if (x > static_cast<unsigned long>(std::numeric_limits<long>::max()))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<long>(x);
}

template<>
inline double lossless_static_cast(unsigned int x)
{
    const auto result = static_cast<double>(x);
    if (x != static_cast<unsigned int>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
inline double lossless_static_cast(long x)
{
    const auto result = static_cast<double>(x);
    if (x != static_cast<long>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
inline double lossless_static_cast(unsigned long x)
{
    const auto result = static_cast<double>(x);
    if (x != static_cast<unsigned long>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
inline int8_t lossless_static_cast(int x)
{
    if (x < std::numeric_limits<int8_t>::min() || x > std::numeric_limits<int8_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<int8_t>(x);
}

template<>
inline uint8_t lossless_static_cast(int x)
{
    if (x < 0 || x > std::numeric_limits<uint8_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint8_t>(x);
}

template<>
inline uint8_t lossless_static_cast(long x)
{
    if (x < 0 || x > std::numeric_limits<uint8_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint8_t>(x);
}

template<>
inline uint8_t lossless_static_cast(uint16_t x)
{
    if (x > std::numeric_limits<uint8_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint8_t>(x);
}

template<>
inline uint16_t lossless_static_cast(int x)
{
    if (x < 0 || x > std::numeric_limits<uint16_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint16_t>(x);
}

template<>
inline uint32_t lossless_static_cast(int x)
{
    if (x < 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint32_t>(x);
}

template<>
inline unsigned long lossless_static_cast(int16_t x)
{
    if (x < 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<unsigned long>(x);
}

template<>
inline unsigned long lossless_static_cast(uint8_t x)
{
    return static_cast<unsigned long>(x);
}

template<>
inline unsigned long lossless_static_cast(uint16_t x)
{
    return static_cast<unsigned long>(x);
}

template<>
inline unsigned long lossless_static_cast(int x)
{
    if (x < 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<unsigned long>(x);
}

template<>
inline unsigned long lossless_static_cast(long x)
{
    if (x < 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<unsigned long>(x);
}

template<>
constexpr double lossless_static_cast(int x)
{
    return static_cast<double>(x);
}

template<>
inline int lossless_static_cast(double x)
{
    double xIntegralPart;
    if (std::modf(x, &xIntegralPart) != 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    is_value_in_type_range<int, true>(xIntegralPart);
    return static_cast<int>(xIntegralPart);
}

template<>
inline long lossless_static_cast(double x)
{
    double xIntegralPart;
    if (std::modf(x, &xIntegralPart) != 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    is_value_in_type_range<long, true>(xIntegralPart);
    return static_cast<long>(xIntegralPart);
}

template<>
constexpr uint32_t lossless_static_cast(uint16_t x)
{
    return static_cast<uint16_t>(x);
}

template<>
constexpr uint64_t lossless_static_cast(uint32_t x)
{
    return static_cast<uint64_t>(x);
}



template<typename T, typename U,
         std::enable_if_t<std::is_same<T, U>::value && std::is_arithmetic<T>::value && std::is_arithmetic<U>::value> * = nullptr>
inline T limited_static_cast(U x)
{
    return x;
}

template<typename T, typename U,
         std::enable_if_t<!std::is_same<T, U>::value && std::is_arithmetic<T>::value && std::is_arithmetic<U>::value> * = nullptr>
T limited_static_cast(U x);



template<>
inline int limited_static_cast(size_t x)
{
    if (x > static_cast<size_t>(std::numeric_limits<int>::max()))
        return std::numeric_limits<int>::max();
    return static_cast<int>(x);
}

template<>
inline uint8_t limited_static_cast(int x)
{
    if (x > static_cast<int>(std::numeric_limits<uint8_t>::max()))
        return std::numeric_limits<uint8_t>::max();
    if (x < static_cast<int>(std::numeric_limits<uint8_t>::min()))
        return std::numeric_limits<uint8_t>::min();
    return static_cast<uint8_t>(x);
}



template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T round_AS(U x)
{
    return lossless_static_cast<T>(std::round(x));
}

template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T floor_AS(U x)
{
    return lossless_static_cast<T>(std::floor(x));
}

template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T ceil_AS(U x)
{
    return lossless_static_cast<T>(std::ceil(x));
}



#define longsizeof(x) (static_cast<long>(sizeof(x)))
#define intsizeof(x) (static_cast<int>(sizeof(x)))
#define checkedlongsizeof(x) (lossless_static_cast<long>(sizeof(x)))
#define checkedintsizeof(x) (lossless_static_cast<int>(sizeof(x)))



constexpr const char* Module_divisor_is_0 = "Can not calculate modulo when divisor is 0";

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
inline int modulo2Power(int value, unsigned powerOf2)
{
    if (powerOf2 == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    return value & static_cast<int>(powerOf2 - 1);
}

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
// Slightly faster than modulo(int, int).
inline int modulo(int value, unsigned m)
{
    if (m == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    int mod = value % static_cast<int>(m);
    if (value < 0) {
        mod += m;
    }
    return mod;
}

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
inline int modulo_euclidean(int value, int m) // modulo_Euclidean2
{
    if (m == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    if (m == -1)
        return 0; // This test needed to prevent UB of `INT_MIN % -1`.
    int mod = value % m;
    if (mod < 0)
        mod = (m < 0) ? mod - m : mod + m;
    return mod;
}

#endif // CPP_HELPERS_H
