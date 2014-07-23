// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include <string>
#include <cassert>

namespace Rpc
{

class Value;

class RequestParser
{
public:

    /// root value must contain following members: method, params. Optional members are: id.
    bool parse(const std::string& request_uri,
               const std::string& request_content,
               Rpc::Value* root)
    {
        assert(root);
        return parse_(request_uri, request_content, root);
    }

protected:

    virtual bool parse_(const std::string& request_uri,
                        const std::string& request_content,
                        Rpc::Value* root) = 0;

    ~RequestParser() {}
};

} // namespace Rpc
