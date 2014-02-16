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

    const std::string& realm() const;

    unsigned long generate_nonce() const;

private:

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Authentication
} // namespace Http