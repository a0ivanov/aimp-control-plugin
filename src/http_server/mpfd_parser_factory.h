#pragma once

#include <boost/filesystem.hpp>

namespace MPFD {
    class Parser;
}

namespace Http {
namespace MPFD {

class ParserFactory {
public:
    virtual std::unique_ptr<::MPFD::Parser> createParser(const std::string& content_type) = 0;


    typedef std::shared_ptr<ParserFactory> ParserFactoryPtr;
    static ParserFactoryPtr instance()
        { return s_instance; }
    static void instance(ParserFactoryPtr factory)
        { s_instance = factory; }

private:

    static ParserFactoryPtr s_instance;
};

class ParserFactoryImpl : public ParserFactory, private boost::noncopyable
{
public:
    ParserFactoryImpl(boost::filesystem::wpath temp_dir)
        :
        temp_dir_(temp_dir)
    {}

    virtual std::unique_ptr<::MPFD::Parser> createParser(const std::string& content_type);

private:

    boost::filesystem::wpath temp_dir_; 
}; 

} // namespace MPFD
} // namespace Http
