// Copyright (c) 2012, Alexey Ivanov

#ifndef RPCEXCEPTION_H
#define RPCEXCEPTION_H

#include <string>

namespace Rpc
{

// Use codes in range [1-10] for Rpc::Value's and request parsing errors. Rpc::Method uses [11-1000] codes.
enum ERROR_CODES { REQUEST_PARSING_ERROR = 1, METHOD_NOT_FOUND_ERROR, TYPE_ERROR, INDEX_RANGE_ERROR, OBJECT_ACCESS_ERROR, VALUE_RANGE_ERROR, INTERNAL_ERROR };

class Exception
{
public:

    explicit Exception(const std::string& message, int code = -1)
        :
        message_(message),
        code_(code)
    {}

    const std::string& message() const
        { return message_; }

    int code() const
        { return code_; }

private:

    std::string message_;
    int code_;
};

} // namespace Rpc

#endif // #ifndef RPCEXCEPTION_H
