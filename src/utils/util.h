// Copyright (c) 2011, Alexey Ivanov

#ifndef UTILITIES_H
#define UTILITIES_H

#include "boost/timer.hpp"
#include <string>
#include <sstream>

typedef unsigned long crc32_t;

//! miscellaneous utils that used by several modules and can not be linked to concrete module.
namespace Utilities
{

//! Limit value by range [min, max].
template <typename T>
T limit_value(T value, T min, T max)
{
    return (value < min) ? min
                         : (value > max) ? max
                                         : value;
}

//! Returns crc32 of buffer[0, length);
crc32_t crc32(const void* buffer, unsigned int length);

//! Type safe version of crc32().
template<class T>
crc32_t crc32(const T& value)
{
    return crc32( &value, sizeof(value) );
}

template<>
inline crc32_t crc32<std::wstring>(const std::wstring& string)
{
    return string.length() > 0 ? crc32( &string[0],
                                        string.length() * sizeof(std::wstring::traits_type::char_type)
                                       )
                               : 0;
}

template<>
inline crc32_t crc32<std::string>(const std::string& string)
{
    return string.length() > 0 ? crc32( &string[0],
                                        string.length() * sizeof(std::string::traits_type::char_type)
                                       )
                               : 0;
}

//! Simple class to log time durations.
class Profiler
{
public:

    // Note, string pointed by msg should be valid till Profiler object destruction.
    Profiler(const char * const msg = "");

    // print duration report in logger.
    ~Profiler();

private:

    boost::timer timer_;
    const char * const msg_; //! prefix of duration report.

    Profiler(const Profiler&);
    Profiler& operator=(const Profiler&);
};

#define PROFILE_EXECUTION_TIME(msg) Utilities::Profiler profiler(msg)

class MakeString {
public:

    template <typename T>
    MakeString& operator<<(const T& object) {
        os_ << object;
        return *this;
    }

    operator std::string() const
        { return os_.str(); }

private:
    std::ostringstream os_;
};


} // namespace Utilities

#endif // #ifndef UTILITIES_H
