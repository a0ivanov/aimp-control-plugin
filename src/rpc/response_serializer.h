// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include <string>

namespace Rpc
{

class Value;

class ResponseSerializer
{
public:

    // Root response can contain following members: result, id.
    virtual void serializeSuccess(const Rpc::Value& root_response, std::string* response) const = 0;

    virtual void serializeFault(const Rpc::Value& root_request, const std::string& error_msg, int error_code, std::string* response) const = 0;

    virtual const std::string& mimeType() const = 0;

protected:

    ~ResponseSerializer() {}
};

} // namespace Rpc
