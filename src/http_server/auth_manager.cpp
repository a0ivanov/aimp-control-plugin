#include "stdafx.h"
#include "http_server/auth_manager.h"
#include "plugin/control_plugin.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/util.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Http::Server>(); }
}

namespace Http
{
namespace Authentication
{

using ControlPlugin::PluginLogger::LogManager;
using namespace Utilities;

const wchar_t * const kPASSWORDS_FILE_NAME = L".htpasswd";

struct HTEntry
{
    std::string user;
    std::string realm;
    std::string ha1;
};
typedef std::vector<HTEntry> HTEntries;

struct AuthManager::Impl
{
    bool enabled_;
    HTEntries ht_entries_;

    Impl()
        : enabled_(false)
    {
        boost::filesystem::wpath password_file_path = ControlPlugin::AIMPControlPlugin::getPluginDirectoryPath();
        password_file_path /= kPASSWORDS_FILE_NAME;

        if (boost::filesystem::exists(password_file_path)) {
            try {
                ht_entries_ = loadAuthData(password_file_path);
                enabled_ = true;
            } catch (std::ios_base::failure&) {
                BOOST_LOG_SEV(logger(), critical) << "Error reading of passwords file at " << password_file_path << ". Reason: bad file format.";
                throw;
            } catch (std::exception& e) {
                BOOST_LOG_SEV(logger(), error) << "Error reading of passwords file at " << password_file_path << ". Reason: " << e.what();
                throw;
            }
        }
    }

    static HTEntries loadAuthData(boost::filesystem::wpath path) // throws std::ios_base::failure, std::runtime_error
    {
        HTEntries entries;

        std::ifstream file(path.native());
        if (!file.is_open()) {
            throw std::runtime_error("Can't open file");
        }

        std::string line;
        std::string user,realm,ha1;

        std::stringstream line_is;
        line_is.exceptions(std::ios_base::failbit | std::ios_base::badbit);

        while (std::getline(file, line, '\n')) {
            line_is.clear();
            line_is.str(line);   

            entries.emplace_back();
            HTEntry& entry = entries.back();

            std::getline(line_is, entry.user,  ':');
            std::getline(line_is, entry.realm, ':');
            std::getline(line_is, entry.ha1       );
        }
        return entries;
    }

    bool enabled() {
        return enabled_;
    }
    

};

AuthManager::AuthManager()
    : impl_(new Impl())
{
}

// it is here to allow using forward declaration of Impl in class definition. Otherwise, undefined dtor error will be generated.
AuthManager::~AuthManager()
{
}

bool AuthManager::enabled() {
    return impl_->enabled();
}

} // namespace Authentication
} // namespace Http
