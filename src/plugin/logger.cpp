// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/util.h"
#include <boost/filesystem.hpp>

#pragma warning (push, 3)
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/formatters.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/filters.hpp>
#pragma warning (pop)

namespace ControlPlugin { namespace PluginLogger {

const std::string LogManager::kRPC_SERVER_MODULE_NAME =   "rpc_server";
const std::string LogManager::kHTTP_SERVER_MODULE_NAME =  "http_server";
const std::string LogManager::kAIMP_MANAGER_MODULE_NAME = "aimp_manager";
const std::string LogManager::kPLUGIN_MODULE_NAME =       "plugin";

LogManager::LogManager()
    : severity_(severity_levels_count),
      plugin_lg_(keywords::channel = kPLUGIN_MODULE_NAME),
      aimp_manager_lg_(keywords::channel = kAIMP_MANAGER_MODULE_NAME),
      rpc_server_lg_(keywords::channel = kRPC_SERVER_MODULE_NAME),
      http_server_lg_(keywords::channel = kHTTP_SERVER_MODULE_NAME)
{}

LogManager::~LogManager()
{
    stopLog();
}

bool LogManager::logInModuleEnabled(const std::string& module_name) const
{
    return modules_to_log_.find(module_name) != modules_to_log_.end();
}

/*
    For using with boost::log::core::set_exception_handler();
    Notice: since exception handler is called from a catch statement, the exception can be rethrown in order to determine its type.
*/
struct BoostLogExceptionHandler
{
    typedef void result_type;

    void operator() (const fs::filesystem_error& /*e*/) const
    {
        //core->remove_sink(synk_file_); // can't do this: deadlock occures.

        assert(!"Filesystem exception in Boost.Log library.");
        //throw;
    }

    void operator() () const
    {
        // unknown exception, just suppress in non debug build.
        assert(!"Unknown exception in Boost.Log library.");
    }
};

void LogManager::checkDirectoryAccess(const boost::filesystem::wpath& log_directory) const
{
    // ensure directory exists or can be created
    try {
        fs::create_directory(log_directory);
    } catch (fs::filesystem_error&) {
        throw FileLogError("Log directory can not be created.");
    }

    boost::filesystem::wpath test_file_path = log_directory / L"testdiraccess";
    // ensure we can create file there
    std::wofstream temp_file( test_file_path.c_str() );
    if ( !temp_file.is_open() ) {
        throw FileLogError("Log directory is read only.");
    }

    temp_file.close();

    // ensure we can remove files
    try {
        remove(test_file_path);
    } catch (fs::filesystem_error&) {
        throw FileLogError("Test file can not be removed. Log directory should provide full access for correct work.");
    }
}

void LogManager::startLog(const fs::wpath& log_directory,
                          const std::set<std::string>& modules_to_log)
{
    namespace attrs = boost::log::attributes;
    namespace fmt = boost::log::formatters;
    namespace flt = boost::log::filters;

    modules_to_log_ = modules_to_log;

    boost::shared_ptr<log::core> core = log::core::get();

    // global settings
    {
        // enable log.
        core->set_logging_enabled(true);

        // Set a global severity filter.
        core->set_filter(flt::attr< SEVERITY_LEVELS >("Severity") >= severity_);

        // add a time stamp attribute.
        core->add_global_attribute("TimeStamp", attrs::local_clock());

        // add a thread ID attribute.
        core->add_global_attribute("ThreadID", attrs::current_thread_id());

        // Setup a global exception handler that will call BoostLogExceptionHandler::operator()
        // for the specified exception types. Note the std::nothrow argument that
        // specifies that all other exceptions should also be passed to the functor.
        core->set_exception_handler(log::make_exception_handler<fs::filesystem_error >
                                                                    (BoostLogExceptionHandler(), std::nothrow)
                                    );
    }

    // use macro since sinks::debug_output_backend need newline character at the end.
    // Thread id format syntax is got from http://sourceforge.net/projects/boost-log/forums/forum/710021/topic/5064327. Old syntax generates exception of first use.
#define AIMP_FORMATTER fmt::stream \
        << fmt::date_time< boost::posix_time::ptime >("TimeStamp", "%d.%m.%Y %H:%M:%S.%f") \
        << " [Thread " << fmt::attr< attrs::current_thread_id::value_type >("ThreadID") << "] " \
        << fmt::attr< SEVERITY_LEVELS >("Severity", std::nothrow) << " " \
        << fmt::attr< std::string >("Channel") << " : " \
        << fmt::message()

    // create debug sink
    {
        sink_dbg_ = boost::make_shared<debug_sink>();

        sink_dbg_->set_formatter(AIMP_FORMATTER << "\n"); // notice, that debug_output_backend requires newline character.

        // Set the special filter to the frontend
        // in order to skip the sink when no debugger is available
        sink_dbg_->set_filter( sinks::debug_output_backend::debugger_presence_filter() );

        core->add_sink(sink_dbg_);
    }

    // create file sink
    {
        checkDirectoryAccess(log_directory); // can throw FileLogError

        boost::shared_ptr< sinks::text_file_backend > backend_file =
            boost::make_shared< sinks::text_file_backend >(
                // file name pattern
                keywords::file_name = log_directory / L"aimp_control_%5N.log",
                // rotate the file upon reaching 5 MiB size...
                keywords::rotation_size = 5 * 1024 * 1024,
                // ...or at noon, whichever comes first
                keywords::time_based_rotation = sinks::file::rotation_at_time_point(12, 0, 0)
            );

        // Set up the file collector
        backend_file->set_file_collector(
            sinks::file::make_collector(
                // rotated logs will be moved here
                keywords::target = log_directory / L"logs",
                // oldest log files will be removed if the total size reaches 10 MiB...
                keywords::max_size = 10 * 1024 * 1024,
                // ...or the free space in the target directory comes down to 50 MiB
                keywords::min_free_space = 50 * 1024 * 1024
            )
        );

        // Look for files that may have left from previous runs of the application
        backend_file->scan_for_files();

        // Enable auto-flushing after each log record written
        backend_file->auto_flush(true);

        sink_file_ = boost::make_shared<sink_file_t>(backend_file);

        sink_file_->set_formatter(AIMP_FORMATTER);

        // add module filter
        sink_file_->set_filter(
            flt::attr< std::string >("Channel").satisfies( boost::bind(&LogManager::logInModuleEnabled, this, _1) )
        );

        core->add_sink(sink_file_);
    }
#undef AIMP_FORMATTER
}

void LogManager::stopLog()
{
    boost::shared_ptr< log::core > core = log::core::get();
    // disable log.
    core->set_logging_enabled(false);
    // remove sinks.
    if (sink_file_) {
        core->remove_sink(sink_file_);
    }
    if (sink_dbg_) {
        core->remove_sink(sink_dbg_);
    }
}

void LogManager::setSeverity(int severity)
{
    severity_ = static_cast<SEVERITY_LEVELS>( Utilities::limit_value<int>( severity,
                                                                           debug,
                                                                           severity_levels_count - 1
                                                                         )
                                            );
}

SEVERITY_LEVELS LogManager::getSeverity() const
{
    return severity_;
}

// The formatting logic for the severity level
template <typename CharT, typename TraitsT>
inline std::basic_ostream< CharT, TraitsT >& operator<< (
    std::basic_ostream< CharT, TraitsT >& strm, SEVERITY_LEVELS severity)
{
    static const char* const str[] = {
        "debug",
        "info",
        "warning",
        "error",
        "critical"
    };
    const std::size_t levels_count = sizeof(str) / sizeof(*str) ;

    if (static_cast<std::size_t>(severity) < levels_count) {
        strm << str[severity];
    } else {
        strm << static_cast<int>(severity);
    }

    return strm;
}

template<>
ModuleLoggerType& LogManager::getModuleLogger<ControlPlugin::AIMPControlPlugin>()
{
    return plugin_lg_;
}

template<>
ModuleLoggerType& LogManager::getModuleLogger<AIMPPlayer::AIMPManager>()
{
    return aimp_manager_lg_;
}

template<>
ModuleLoggerType& LogManager::getModuleLogger<Rpc::RequestHandler>()
{
    return rpc_server_lg_;
}

template<>
ModuleLoggerType& LogManager::getModuleLogger<Http::Server>()
{
    return http_server_lg_;
}

} } // namespace ControlPlugin::PluginLogger
