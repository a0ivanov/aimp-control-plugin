// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "rpc/request_parser.h"

namespace Rpc { class Value; }

namespace XmlRpc
{

class RequestParser : public Rpc::RequestParser
{
    virtual bool parse_(const std::string& /*request_uri*/,
                        const std::string& request_content,
                        Rpc::Value* root);
};

} // namespace XmlRpc
