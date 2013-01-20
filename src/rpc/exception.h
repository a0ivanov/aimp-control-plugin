// Copyright (c) 2013, Alexey Ivanov

#ifndef RPCEXCEPTION_H
#define RPCEXCEPTION_H

#include <string>

/*! 
    #ERROR_CODES \internal This must be mentioned to proper generation links to values of this enum \endinternal
*/
namespace Rpc
{

/*!
    Errors occures on RPC method calling process.

    \internal Use codes in range [1-10] for Rpc::Value's and request parsing errors. Rpc::Method uses [11-1000] codes. \endinternal
*/
enum ERROR_CODES { 
    REQUEST_PARSING_ERROR = 1, /*!< Request parsing error. */
    METHOD_NOT_FOUND_ERROR = 2, /*!< RPC method does not supported. */
    TYPE_ERROR = 3, /*!< One or more argumets have unexpected types. */
    INDEX_RANGE_ERROR = 4, /*!< Value of index argumet is out of range. */
    OBJECT_ACCESS_ERROR = 5, /*!< Object does not have requested field. */
    VALUE_RANGE_ERROR = 6, /*!< Value of argument is out of range. */
    INTERNAL_ERROR = 7 /*!< Unknown error. */
};

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
