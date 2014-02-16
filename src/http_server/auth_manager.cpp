#include "stdafx.h"
#include "http_server/auth_manager.h"
#include "http_server/request.h"
#include "http_server/request_parser.h"
#include "http_server/mongoose/mongoose.h"
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
    static const std::string kHEADER_AUTHORIZATION_NAME;
    std::string realm_;

    Impl()
        : enabled_(false),
        realm_("AIMP Control plugin")
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

    bool enabled() const {
        return enabled_;
    }
    
    bool isAuthenticated(const Request& req) const {
        const std::string* autorization_value;
        if (!get_header_value(req.headers, kHEADER_AUTHORIZATION_NAME, autorization_value)) {
            return false;
        }
        const char* hdr = autorization_value->c_str();

        if (mg_strncasecmp(hdr, "Digest ", 7) != 0) return 0;

        const int MAX_REQUEST_SIZE = 16384;
        char user[100], nonce[100], uri[MAX_REQUEST_SIZE], cnonce[100], resp[100], qop[100], nc[100];

        if (!mg_parse_header(hdr, "username", user, sizeof(user))) return 0;
        if (!mg_parse_header(hdr, "cnonce", cnonce, sizeof(cnonce))) return 0;
        if (!mg_parse_header(hdr, "response", resp, sizeof(resp))) return 0;
        if (!mg_parse_header(hdr, "uri", uri, sizeof(uri))) return 0;
        if (!mg_parse_header(hdr, "qop", qop, sizeof(qop))) return 0;
        if (!mg_parse_header(hdr, "nc", nc, sizeof(nc))) return 0;
        if (!mg_parse_header(hdr, "nonce", nonce, sizeof(nonce))) return 0;

        for(auto ht_entry : ht_entries_) {
            if ( ht_entry.user == user && ht_entry.realm == realm() ) {
                return check_password(req.method.c_str(), ht_entry.ha1.c_str(), uri, nonce, nc, cnonce, qop, resp) == MG_AUTH_OK;
            }
        }
        return false;
    }

    const std::string& realm() const {
        return realm_;
    }

    unsigned long generate_nonce() const {
        return (unsigned long)time(nullptr);
    }
};

const std::string AuthManager::Impl::kHEADER_AUTHORIZATION_NAME = "Authorization";

AuthManager::AuthManager()
    : impl_(new Impl())
{
}

// it is here to allow using forward declaration of Impl in class definition. Otherwise, undefined dtor error will be generated.
AuthManager::~AuthManager()
{
}

bool AuthManager::enabled() const {
    return impl_->enabled();
}

bool AuthManager::isAuthenticated(const Request& req) const {
    return impl_->isAuthenticated(req);
}

const std::string& AuthManager::realm() const {
    return impl_->realm();
}

unsigned long AuthManager::generate_nonce() const {
    return impl_->generate_nonce();
}

} // namespace Authentication
} // namespace Http
