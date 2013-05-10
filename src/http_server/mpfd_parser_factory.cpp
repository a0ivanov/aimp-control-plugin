#include "stdafx.h"

#include "mpfd_parser_factory.h"
#include "mpfd_parser/Parser.h"
#include "utils/string_encoding.h"

namespace Http {
namespace MPFD {

ParserFactory::ParserFactoryPtr ParserFactory::s_instance;

std::unique_ptr<::MPFD::Parser> ParserFactoryImpl::createParser(const std::string& content_type)
{
    std::unique_ptr<::MPFD::Parser> parser(new ::MPFD::Parser);
    parser->SetUploadedFilesStorage(::MPFD::Parser::StoreUploadedFilesInFilesystem);
    parser->SetTempDirForFileUpload(StringEncoding::utf16_to_system_ansi_encoding(temp_dir_.native()));
    parser->SetContentType(content_type);

    return parser;
}

} // namespace MPFD
} // namespace Http