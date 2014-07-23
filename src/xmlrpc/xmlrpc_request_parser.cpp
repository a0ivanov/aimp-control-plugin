// Copyright (c) 2014, Alexey Ivanov

#include "stdafx.h"
#include "xmlrpc/request_parser.h"
#include "xmlrpc/parse_util.h"
#include "xmlrpc/value.h"
#include "rpc/value.h"
#include "rpc/exception.h"

namespace XmlRpc
{

const char * const METHODNAME_TAG = "<methodName>";
const char * const PARAMS_TAG     = "<params>";
const char * const PARAMS_ETAG    = "</params>";
const char * const PARAM_TAG      = "<param>";
const char * const PARAM_ETAG     = "</param>";

//const std::string SYSTEM_MULTICALL = "system.multicall";
//const std::string METHODNAME = "methodName";
//const std::string PARAMS     = "params";

void convertXmlRpcValueToRpcValue(const Value& xml_rpc_value, Rpc::Value* rpc_value); // throws Rpc::Exception

bool RequestParser::parse_(const std::string& /*request_uri*/,
                           const std::string& request_content,
                           Rpc::Value* root)
{
    std::size_t offset = 0; // Number of chars parsed from the request
    (*root)["method"] = Util::parseTag(METHODNAME_TAG, request_content, &offset);
    const std::string& method_name = (*root)["method"];
    if (  method_name.size() > 0 && Util::findTag(PARAMS_TAG, request_content, &offset) ) {
        Value params;
        std::size_t argc = 0;
        while ( Util::nextTagIs(PARAM_TAG, request_content, &offset) ) {
            params[argc++] = Value(request_content, &offset);
            Util::nextTagIs(PARAM_ETAG, request_content, &offset);
        }
        if ( Util::nextTagIs(PARAMS_ETAG, request_content, &offset) ) {
            convertXmlRpcValueToRpcValue(params, &(*root)["params"]);
            return true;
        }
    }
    return false;
}

void convertXmlRpcValueToRpcValue(const Value& xml_rpc_value, Rpc::Value* rpc_value) // throws Rpc::Exception
{
    assert(rpc_value);

    switch ( xml_rpc_value.getType() ) {
    case Value::TypeInvalid:
        Rpc::Value().swap(*rpc_value);
        break;
    case Value::TypeNil:
        Rpc::Value( Rpc::Value::Null() ).swap(*rpc_value);
        break;
    case Value::TypeBoolean:
        *rpc_value = bool(xml_rpc_value);
        break;
    case Value::TypeInt:
        *rpc_value = int(xml_rpc_value);
        break;
    case Value::TypeDouble:
        *rpc_value = double(xml_rpc_value);
        break;
    case Value::TypeString:
        *rpc_value = static_cast<const std::string&>(xml_rpc_value);
        break;
    case Value::TypeArray:
        rpc_value->setSize( xml_rpc_value.size() );
        for (size_t i = 0, size = rpc_value->size(); i != size; ++i) {
            convertXmlRpcValueToRpcValue(xml_rpc_value[i], &((*rpc_value)[i]));
        }
        break;
    case Value::TypeStruct:
        {
        Rpc::Value::Object object;
        auto member_it = xml_rpc_value.getStructMembersBegin(),
             end       = xml_rpc_value.getStructMembersEnd();
        for (; member_it != end; ++member_it) {
            convertXmlRpcValueToRpcValue(member_it->second, &object[member_it->first]);
        }

        *rpc_value = object;
        }
        break;
    case Value::TypeDateTime:
    case Value::TypeBase64:
        assert(!"Value's DateTime and Base64 types are not supported.");
    default:
        throw Rpc::Exception("unknown type", Rpc::TYPE_ERROR);
    }
}

} // namespace XmlRpc
