// Copyright (c) 2014, Alexey Ivanov

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

namespace ControlPlugin { namespace PluginSettings {

using namespace PluginLogger;
using boost::property_tree::wptree;

static std::wstring kDEFAULT_REALM = L"AIMP Control plugin";
static std::wstring kDEFAULT_PORT = L"3333";

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
    s.interfaces.clear();
    s.interfaces.insert(Settings::HttpServer::NetworkInterface("", "localhost", StringEncoding::utf16_to_system_ansi_encoding_safe(kDEFAULT_PORT)));
    s.document_root = L"htdocs";
    s.realm = kDEFAULT_REALM;
}

void loadPropertyTreeFromFile(wptree& pt, const boost::filesystem::wpath& filename) // throws std::exception
{
    std::wifstream istream;
    istream.exceptions(std::ios_base::failbit | std::ios_base::badbit); // ask stream to generate exception in case of error.
    istream.open( filename.c_str() );

    // set UTF-8 convert facet.
    istream.imbue( std::locale(istream.getloc(),
                                new boost::archive::detail::utf8_codecvt_facet
                                )
                    );

    read_xml(istream, pt); // Load XML file and put its contents in property tree.
    istream.close();
}

int severityLevelFromString(const std::wstring& level_name);

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
    const int log_severity_level = severityLevelFromString( pt.get<std::wstring>(L"settings.logging.severity",
                                                                                 L"critical")
                                                           );

    std::set<std::string> modules_to_log;

    using namespace StringEncoding;

    // Iterate over logging.modules section and store all found
    // modules in modules_to_log_ set. get_child() function returns a
    // reference to child at specified path; if there is no such
    // child, it throws. Property tree iterator can be used in
    // the same way as standard container iterator. Category
    // is bidirectional_iterator.
    for ( const auto& v : pt.get_child(L"settings.logging.modules") ) {
        modules_to_log.insert( utf16_to_system_ansi_encoding( v.second.data() ) );
    }

    std::string all_interfaces_port;
    all_interfaces_port = utf16_to_system_ansi_encoding( pt.get<std::wstring>(L"settings.httpserver.all_interfaces.<xmlattr>.port", L"") );

    std::set<Settings::HttpServer::NetworkInterface> interfaces;
    {
        auto loadInterfaceData = [&interfaces](const wptree& tree) {
            std::string mac  = utf16_to_system_ansi_encoding( tree.get<std::wstring>(L"mac", L"") );
            std::string ip   = utf16_to_system_ansi_encoding( tree.get<std::wstring>(L"ip", L"") );
            std::string port = utf16_to_system_ansi_encoding( tree.get<std::wstring>(L"port", L"") );

            Settings::HttpServer::NetworkInterface i(mac, ip, port);
            if ((!i.ip.empty() || !i.mac.empty()) && !i.port.empty()) {
                interfaces.insert(i);
            }
        };

        wptree default_tree;
        for ( const auto& v : pt.get_child(L"settings.httpserver.interfaces", default_tree) ) {
            if (v.first == L"interface") {
                loadInterfaceData(v.second);
            }
        }
    }

    if (interfaces.empty() && all_interfaces_port.empty()) { // check deprecated values only if there was no other settings.
        std::string ip = utf16_to_system_ansi_encoding( pt.get<std::wstring>(L"settings.httpserver.ip_to_bind", L"localhost") );
        std::string port = utf16_to_system_ansi_encoding( pt.get<std::wstring>(L"settings.httpserver.port", kDEFAULT_PORT) );

        if (ip.empty()) { // support old behavior: empty ip means all interfaces.
            all_interfaces_port = port;
        } else {
            interfaces.emplace("", ip, port);
        }
    }

    std::wstring server_document_root = pt.get<std::wstring>(L"settings.httpserver.document_root");
    
    std::wstring realm = pt.get<std::wstring>(L"settings.httpserver.realm", kDEFAULT_REALM);

    std::set<std::string> init_cookies;
    try {
        for ( const auto& v : pt.get_child(L"settings.httpserver.init_cookies") ) {
            init_cookies.insert( utf16_to_system_ansi_encoding( v.second.data() ) );
        }
    } catch (...) {
        // do nothing, it is optional settings.
        init_cookies.clear();
    }

    std::wstring tmp = pt.get<std::wstring>(L"settings.misc.enable_track_upload", L"false");
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    bool enable_track_upload = tmp == L"true" || tmp == L"1";

    tmp = pt.get<std::wstring>(L"settings.misc.enable_physical_track_deletion", L"false");
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    bool enable_physical_track_deletion = tmp == L"true" || tmp == L"1";

    tmp = pt.get<std::wstring>(L"settings.misc.enable_sheduler", L"false");
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    bool enable_sheduler = tmp == L"true" || tmp == L"1";
    
    // all work has been done, save result.
    using std::swap;
    settings.http_server.all_interfaces.port.swap(all_interfaces_port);
    settings.http_server.interfaces.swap(interfaces);
    settings.http_server.document_root.swap(server_document_root);
    settings.http_server.init_cookies.swap(init_cookies);
    settings.http_server.realm.swap(realm);

    settings.logger.severity_level = log_severity_level;
    settings.logger.directory.swap(log_directory);
    settings.logger.modules_to_log.swap(modules_to_log);

    settings.misc.enable_track_upload = enable_track_upload;
    settings.misc.enable_physical_track_deletion = enable_physical_track_deletion;
    settings.misc.enable_sheduler = enable_sheduler;
}

void Manager::load(const boost::filesystem::wpath& filename)
{
    try {
        wptree pt;
        loadPropertyTreeFromFile(pt, filename);
        loadSettingsFromPropertyTree(settings_, pt);
    } catch (std::exception& e) {
        // do nothing here, reading of file config failed, so default values(which were set in ctor) will be used.
        (void)e;
    }
}

void savePropertyTreeToFile(const wptree& pt, const boost::filesystem::wpath& filename) // throws std::exception
{
    std::wofstream ostream;
    ostream.open(filename.c_str(), std::ios_base::out);
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

const wchar_t* severityToString(int level);

void saveSettingsToPropertyTree(const Settings& settings, wptree& pt) // throws std::exception
{
    using namespace StringEncoding;

    if (settings.http_server.all_interfaces.enabled()) {
        pt.put(L"settings.httpserver.all_interfaces.<xmlattr>.port", system_ansi_encoding_to_utf16(settings.http_server.all_interfaces.port)); 
    }

    for (const auto& i: settings.http_server.interfaces) {
        wptree pt_i;
        {
            if (!i.mac.empty()) {
                pt_i.put(L"mac", system_ansi_encoding_to_utf16(i.mac));
            }
            if (!i.ip.empty()) {
                pt_i.put(L"ip", system_ansi_encoding_to_utf16(i.ip));
            }
            pt_i.put(L"port", system_ansi_encoding_to_utf16(i.port));
        }
        pt.add_child( L"settings.httpserver.interfaces.interface", pt_i);
    }

    pt.put( L"settings.httpserver.document_root", settings.http_server.document_root );
    pt.put( L"settings.httpserver.realm", settings.http_server.realm );

    pt.put(L"settings.misc.enable_track_upload", settings.misc.enable_track_upload);
    pt.put(L"settings.misc.enable_physical_track_deletion", settings.misc.enable_physical_track_deletion);
    pt.put(L"settings.misc.enable_sheduler", settings.misc.enable_sheduler);

    // Put log directory in property tree
    pt.put(L"settings.logging.directory", settings.logger.directory);

    // Put log severity in property tree
    pt.put( L"settings.logging.severity", severityToString(settings.logger.severity_level) );

    for (const std::string& name: settings.logger.modules_to_log) {
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

namespace {
    const wchar_t * const severity_levels[] = {
        L"debug",
        L"info",
        L"warning",
        L"error",
        L"critical",
        L"none"
    };
    const size_t levels_count = sizeof(severity_levels) / sizeof(*severity_levels);
}

int severityLevelFromString(const std::wstring& level_name)
{
    for (size_t i = 0; i != levels_count; ++i) {
        if (level_name == severity_levels[i]) {
            return debug + i;
        }
    }
    return severity_levels_count;
}

const wchar_t* severityToString(int level)
{
    if (static_cast<std::size_t>(level) < levels_count) {
        return severity_levels[level];
    }
    return severity_levels[severity_levels_count];
}

} } // namespace ControlPlugin::PluginSettings
