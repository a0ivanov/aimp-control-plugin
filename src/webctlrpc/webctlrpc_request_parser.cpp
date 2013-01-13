// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "webctlrpc/request_parser.h"
#include "rpc/value.h"
#include "rpc/exception.h"
#include <string.h>
#include <boost/lexical_cast.hpp>

namespace WebCtlRpc
{

typedef std::map<std::string, std::string> Arguments;
bool getArgs(const std::string& uri, Arguments* args);
void convertQueryArgumentsToRPCValueParams(const Arguments& args, Rpc::Value& params);

bool RequestParser::parse_(const std::string& request_uri,
                           const std::string& /*request_content*/,
                           Rpc::Value* root)
{
    Arguments args;
    if ( getArgs(request_uri, &args) ) {
        (*root)["method"] = "EmulationOfWebCtlPlugin"; // use single RPC method for entire web_ctl plugin functionality.
        convertQueryArgumentsToRPCValueParams(args, (*root)["params"]);
        return true;
    }
    return false;
}

void skipCharacter(std::istringstream& is, char char_to_skip)
{
    for (;;) {
        int c = is.get();
        if ( !is.good() ) {
            break;
        } else {
            if (c != char_to_skip) {
                is.unget();
                break;
            }
        }
    }
}

bool getArgs(const std::string& uri, Arguments* args)
{
    assert(args);

    std::string arg_name;
    std::istringstream istream(uri);
    skipCharacter(istream, '/');
    skipCharacter(istream, '?'); // skip '/?' or '//?' characters. Aimp UControl sends '//?' for some reason - handle it.
    while ( istream.good() ) {
        getline(istream, arg_name, '=');
        if ( !istream.good() ) {
            return false;
        }
        getline(istream, (*args)[arg_name], '&');
    }
    return istream.eof(); // treat operation successfull if we have read till end of stream.
}

void convertQueryArgumentsToRPCValueParams(const Arguments& args, Rpc::Value& params)
{
    for (auto arg_it = args.begin(),
              end    = args.end();
              arg_it != end;
              ++arg_it
         )
    {
        const std::string& value = arg_it->second;
        // Prevent rpc value's type errors in EmulationOfWebCtlPlugin() rpc method: use integer type if possible.
        // Strange but original webctl client code sends integer params as strings.
        try {
            params[arg_it->first] = boost::lexical_cast<int>(value); // as int
        } catch(boost::bad_lexical_cast&) {
            try {
                params[arg_it->first] = boost::lexical_cast<unsigned int>(value); // as uint
            } catch(boost::bad_lexical_cast&) {
                params[arg_it->first] = value; // as string
            }
        }
    }
}

} // namespace WebCtlRpc
