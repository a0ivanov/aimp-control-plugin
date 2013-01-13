// Copyright (c) 2013, Alexey Ivanov

#ifndef RPC_REQUEST_PARSER_H
#define RPC_REQUEST_PARSER_H

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

#endif // #ifndef RPC_REQUEST_PARSER_H
