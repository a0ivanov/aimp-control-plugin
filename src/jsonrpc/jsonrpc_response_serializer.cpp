// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "jsonrpc/response_serializer.h"
#include "jsonrpc/value.h"
#include "jsonrpc/writer.h"
#include "rpc/value.h"
#include "rpc/exception.h"
#include <cassert>

namespace JsonRpc
{

const std::string kMIME_TYPE = "application/json";

struct ResponseSerializerImpl {
    Json::FastWriter writer;
};

ResponseSerializer::ResponseSerializer()
    :
    impl_(new ResponseSerializerImpl)
{}

ResponseSerializer::~ResponseSerializer()
{
    delete impl_;
}

void convertRpcValueToJsonRpcValue(const Rpc::Value& rpc_value, Json::Value* json_rpc_value); // throws Rpc::Exception

void ResponseSerializer::serializeSuccess(const Rpc::Value& root_response, std::string* response) const
{
    Json::Value jsonrpc_response;
    convertRpcValueToJsonRpcValue(root_response, &jsonrpc_response);
    jsonrpc_response["jsonrpc"] = "2.0";

    *response = impl_->writer.write(jsonrpc_response);
}

void ResponseSerializer::serializeFault(const Rpc::Value& root_request, const std::string& error_msg, int error_code, std::string* response) const
{
    Json::Value response_value;

    if ( root_request.isMember("id") ) {
        convertRpcValueToJsonRpcValue(root_request["id"], &response_value["id"]);
    } else {
        response_value["id"] = Json::Value::null;
    }

    response_value["jsonrpc"] = "2.0";

    Json::Value& error = response_value["error"];
    error["message"] = error_msg;
    error["code"] = error_code;

    *response = impl_->writer.write(response_value);
}

const std::string& ResponseSerializer::mimeType() const
{
    return kMIME_TYPE;
}

void convertRpcValueToJsonRpcValue(const Rpc::Value& rpc_value, Json::Value* json_rpc_value) // throws Rpc::Exception
{
    assert(json_rpc_value);

    switch ( rpc_value.type() ) {
    case Rpc::Value::TYPE_NONE:
        // treat none rpc value as null json value. ///???
    case Rpc::Value::TYPE_NULL:
        Json::Value().swap(*json_rpc_value);
        break;
    case Rpc::Value::TYPE_BOOL:
        *json_rpc_value = bool(rpc_value);
        break;
    case Rpc::Value::TYPE_INT:
        *json_rpc_value = int(rpc_value);
        break;
    case Rpc::Value::TYPE_UINT:
        *json_rpc_value = unsigned int(rpc_value);
        break;
    case Rpc::Value::TYPE_DOUBLE:
        *json_rpc_value = double(rpc_value);
        break;
    case Rpc::Value::TYPE_STRING:
        *json_rpc_value = static_cast<const std::string&>(rpc_value);
        break;
    case Rpc::Value::TYPE_ARRAY:
        json_rpc_value->resize( rpc_value.size() );
        for (size_t i = 0, size = json_rpc_value->size(); i != size; ++i) {
            convertRpcValueToJsonRpcValue(rpc_value[i], &((*json_rpc_value)[i]));
        }
        break;
    case Rpc::Value::TYPE_OBJECT:
        {
        *json_rpc_value = Json::Value();
        auto member_it = rpc_value.getObjectMembersBegin(),
             end       = rpc_value.getObjectMembersEnd();
        for (; member_it != end; ++member_it) {
            convertRpcValueToJsonRpcValue( member_it->second, &((*json_rpc_value)[member_it->first]) );
        }
        }
        break;
    default:
        throw Rpc::Exception("unknown type", Rpc::TYPE_ERROR);
    }
}

} // namespace JsonRpc
