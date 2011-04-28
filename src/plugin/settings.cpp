// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "settings.h"
#include "utils/string_encoding.h"
#include "plugin/logger.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/assign/std.hpp>
#include <libs/serialization/src/utf8_codecvt_facet.cpp>

namespace AIMPControlPlugin { namespace PluginSettings {

using namespace PluginLogger;
using boost::property_tree::wptree;

Manager::Manager()
{
    setDefaultLoggerSettings();
    setDefaultHttpServerSettings();
}

void Manager::setDefaultLoggerSettings()
{
    Settings::Logger& s = settings_.logger;
    s.severity_level =
#ifdef _DEBUG
                        debug
#else
                        critical
#endif
    ;
    s.directory = L"./";
    boost::assign::insert(s.modules_to_log)
        (LogManager::kHTTP_SERVER_MODULE_NAME)
        (LogManager::kRPC_SERVER_MODULE_NAME)
        (LogManager::kAIMP_MANAGER_MODULE_NAME)
        (LogManager::kPLUGIN_MODULE_NAME);
}

void Manager::setDefaultHttpServerSettings()
{
    Settings::HttpServer& s = settings_.http_server;
    s.ip_to_bind = "localhost";
    s.port = "3333";
    s.document_root = L"htdocs";
}

void loadPropertyTreeFromFile(wptree& pt, const boost::filesystem::wpath& filename) // throws std::exception
{
    std::wifstream istream;
    istream.exceptions(std::ios_base::failbit | std::ios_base::badbit); // ask stream to generate exception in case of error.
    istream.open( filename.file_string().c_str() );

    // set UTF-8 convert facet.
    istream.imbue( std::locale(istream.getloc(),
                                new boost::archive::detail::utf8_codecvt_facet
                                )
                    );

    read_xml(istream, pt); // Load XML file and put its contents in property tree.
    istream.close();
}

void loadSettingsFromPropertyTree(Settings& settings, const wptree& pt) // throws std::exception
{
    // Get directory and store it in log_directory_ variable. Note that
    // we specify a path to the value using notation where keys
    // are separated with dots (different separator may be used
    // if keys themselves contain dots). If logging.directory key is
    // not found, exception is thrown.
    std::wstring log_directory = pt.get<std::wstring>(L"settings.logging.directory"); ///??? investigate if we can use default value here. Currently exception is thrown.

    // Get log severity and store it in log_severity_level variable. This is
    // another version of get method: if logging.severity key is not
    // found, it will return default value (specified by second
    // parameter) instead of throwing.
    const int log_severity_level = pt.get<int>(L"settings.logging.severity", critical);

    std::set<std::string> modules_to_log;

    using namespace StringEncoding;

    // Iterate over logging.modules section and store all found
    // modules in modules_to_log_ set. get_child() function returns a
    // reference to child at specified path; if there is no such
    // child, it throws. Property tree iterator can be used in
    // the same way as standard container iterator. Category
    // is bidirectional_iterator.
    BOOST_FOREACH( const wptree::value_type& v, pt.get_child(L"settings.logging.modules") ) {
        modules_to_log.insert( utf16_to_system_ansi_encoding( v.second.data() ) );
    }

    std::string server_ip_to_bind = utf16_to_system_ansi_encoding( pt.get<std::wstring>(L"settings.httpserver.ip_to_bind") );
    std::string server_port = utf16_to_system_ansi_encoding( pt.get<std::wstring>(L"settings.httpserver.port") );

    std::wstring server_document_root = pt.get<std::wstring>(L"settings.httpserver.document_root");

    // all work has been done, save result.
    using std::swap;
    settings.http_server.ip_to_bind.swap(server_ip_to_bind);
    settings.http_server.port.swap(server_port);
    settings.http_server.document_root.swap(server_document_root);

    settings.logger.severity_level = log_severity_level;
    settings.logger.directory.swap(log_directory);
    settings.logger.modules_to_log.swap(modules_to_log);
}

void Manager::load(const boost::filesystem::wpath& filename)
{
    try {
        wptree pt;
        loadPropertyTreeFromFile(pt, filename);
        loadSettingsFromPropertyTree(settings_, pt);
    } catch (std::exception&) {
        // do nothing here, reading of file config failed, so default values(which were set in ctor) will be used.
    }
}

void savePropertyTreeToFile(const wptree& pt, const boost::filesystem::wpath& filename) // throws std::exception
{
    std::wofstream ostream;
    ostream.open(filename.file_string().c_str(), std::ios_base::out);
    ostream.exceptions(std::ios_base::failbit | std::ios_base::badbit); // ask stream to generate exception in case of error.

    // set UTF-8 convert facet.
    ostream.imbue( std::locale(ostream.getloc(),
                               new boost::archive::detail::utf8_codecvt_facet
                               )
                  );

    // format of xml: indent count and character. Use 4 spaces for indent.
    const wptree::key_type::value_type indent_char = L' ';
    const wptree::key_type::size_type indent_count = 4;
    using boost::property_tree::xml_parser::xml_writer_make_settings;
    write_xml( ostream, pt, xml_writer_make_settings(indent_char, indent_count) ); // Write property tree to XML file
    ostream.close();
}

void saveSettingsToPropertyTree(const Settings& settings, wptree& pt) // throws std::exception
{
    using namespace StringEncoding;
    pt.put( L"settings.httpserver.ip_to_bind", system_ansi_encoding_to_utf16(settings.http_server.ip_to_bind) );
    pt.put( L"settings.httpserver.port", system_ansi_encoding_to_utf16(settings.http_server.port) );
    pt.put( L"settings.httpserver.document_root", settings.http_server.document_root );

    // Put log directory in property tree
    pt.put(L"settings.logging.directory", settings.logger.directory);

    // Put log severity in property tree
    pt.put(L"settings.logging.severity", settings.logger.severity_level);

    BOOST_FOREACH(const std::string& name, settings.logger.modules_to_log) {
        pt.add( L"settings.logging.modules.module", system_ansi_encoding_to_utf16(name) ); // add() method is used instead put() since put() add only first module for some reason.
    }
}

void Manager::save(const boost::filesystem::wpath& filename) const
{
    try {
        wptree pt;
        saveSettingsToPropertyTree(settings_, pt);
        savePropertyTreeToFile(pt, filename);
    } catch (std::exception& e) {
        throw SaveError( std::string("Saving settings file failed. Reason: ") + e.what() );
    }
}

} } // namespace AIMPControlPlugin::PluginSettings
