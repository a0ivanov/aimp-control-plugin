// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "jsonrpc/request_parser.h"
#include "jsonrpc/value.h"
#include "rpc/value.h"
#include "rpc/exception.h"
#include "jsonrpc/reader.h"

namespace JsonRpc
{

struct RequestParserImpl {
    Json::Reader reader;
};

RequestParser::RequestParser()
    :
    impl_(new RequestParserImpl)
{}

RequestParser::~RequestParser()
{
    delete impl_;
}

void convertJsonRpcValueToRpcValue(const Json::Value& json_rpc_value, Rpc::Value* rpc_value); // throws Rpc::Exception

bool RequestParser::parse_(const std::string& /*request_uri*/,
                           const std::string& request_content,
                           Rpc::Value* root)
{
    Json::Value json_root;

    if ( impl_->reader.parse(request_content, json_root) ) {
        convertJsonRpcValueToRpcValue(json_root, root);
        return true;
    }
    return false;
}

void convertJsonRpcValueToRpcValue(const Json::Value& json_rpc_value, Rpc::Value* rpc_value) // throws Rpc::Exception
{
    assert(rpc_value);

    switch ( json_rpc_value.type() ) {
    case Json::nullValue:
        Rpc::Value( Rpc::Value::Null() ).swap(*rpc_value);
        break;
    case Json::booleanValue:
        *rpc_value = json_rpc_value.asBool();
        break;
    case Json::uintValue:
        *rpc_value = json_rpc_value.asUInt();
        break;
    case Json::intValue:
        *rpc_value = json_rpc_value.asInt();
        break;
    case Json::realValue:
        *rpc_value = json_rpc_value.asDouble();
        break;
    case Json::stringValue:
        *rpc_value = json_rpc_value.asString();
        break;
    case Json::arrayValue:
        rpc_value->setSize( json_rpc_value.size() );
        for (size_t i = 0, size = rpc_value->size(); i != size; ++i) {
            convertJsonRpcValueToRpcValue(json_rpc_value[i], &((*rpc_value)[i]));
        }
        break;
    case Json::objectValue:
        {
        Rpc::Value::Object object;

        const Json::Value::Members& member_names = json_rpc_value.getMemberNames();
        auto member_name_it = member_names.begin(),
             end            = member_names.end();
        for (; member_name_it != end; ++member_name_it) {
            const std::string& member_name = *member_name_it;
            convertJsonRpcValueToRpcValue(json_rpc_value[member_name], &object[member_name]);
        }
        *rpc_value = object;
        }
        break;
    default:
        throw Rpc::Exception("unknown type", Rpc::TYPE_ERROR);
    }
}

} // namespace JsonRpc
