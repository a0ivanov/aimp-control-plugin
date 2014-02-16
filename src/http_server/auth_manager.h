#pragma once

#include <boost/noncopyable.hpp>

namespace Http
{
namespace Authentication
{

class AuthManager : private boost::noncopyable
{
public:
    AuthManager();
    ~AuthManager();

    bool enabled();

private:

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Authentication
} // namespace Http