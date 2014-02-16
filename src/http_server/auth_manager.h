#pragma once

#include <boost/noncopyable.hpp>

namespace Http
{

struct Request;

namespace Authentication
{

class AuthManager : private boost::noncopyable
{
public:
    AuthManager();
    ~AuthManager();

    bool enabled() const;

    bool isAuthenticated(const Request&) const; 

private:

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Authentication
} // namespace Http