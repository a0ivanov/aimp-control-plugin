// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "control_plugin.h"
#include "logger.h"
#include "settings.h"
#include "rpc/methods.h"
#include "rpc/frontend.h"
#include "rpc/request_handler.h"
#include "xmlrpc/frontend.h"
#include "jsonrpc/frontend.h"
#include "webctlrpc/frontend.h"
#include "http_server/request_handler.h"
#include "http_server/request_handler.h"
#include "http_server/server.h"
#include "utils/string_encoding.h"

#include "rpc/multi_user_mode_manager.h"

#include <FreeImagePlus.h>
#include <Delayimp.h>

/* Plugin DLL export function that will be called by AIMP. */
BOOL WINAPI AIMP_QueryAddonEx(AIMP2SDK::IAIMPAddonHeader **newAddon)
{
    using AIMPControlPlugin::plugin_instance;
    plugin_instance = new AIMPControlPlugin::AIMPControlPluginHeader();
    plugin_instance->AddRef();
    *newAddon = plugin_instance;
    return TRUE;
}

namespace {
using namespace AIMPControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPControlPlugin::AIMPControlPluginHeader>(); }
}

namespace AIMPControlPlugin
{

const std::wstring AIMPControlPluginHeader::kPLUGIN_SHORT_NAME        = L"Control Plugin";
const std::wstring AIMPControlPluginHeader::kPLUGIN_AUTHOR            = L"Alexey Ivanov";
const std::wstring AIMPControlPluginHeader::kPLUGIN_SETTINGS_FILENAME = L"settings.dat";

AIMPControlPluginHeader* plugin_instance = NULL;

namespace PluginLogger
{

PluginLogger::LogManager& getLogManager()
{
    return AIMPControlPluginHeader::getLogManager();
}

} // namespace AIMPControlPlugin

PluginLogger::LogManager& AIMPControlPluginHeader::getLogManager()
{
    return plugin_instance->plugin_logger_;
}

AIMPControlPluginHeader::AIMPControlPluginHeader()
    :
    free_image_dll_is_available_(false)
{
}

BOOL WINAPI AIMPControlPluginHeader::GetHasSettingsDialog()
{
    return TRUE;
}

PWCHAR WINAPI AIMPControlPluginHeader::GetPluginAuthor()
{
    return const_cast<const PWCHAR>( kPLUGIN_AUTHOR.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
}

PWCHAR WINAPI AIMPControlPluginHeader::GetPluginName()
{
    return const_cast<const PWCHAR>( kPLUGIN_SHORT_NAME.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
}

boost::filesystem::wpath AIMPControlPluginHeader::getSettingsFilePath()
{
    return plugin_work_directory_ / kPLUGIN_SETTINGS_FILENAME;
}

boost::filesystem::wpath AIMPControlPluginHeader::makePluginWorkDirectory()
{
    using namespace AIMP2SDK;
    // by default return ".\kPLUGIN_SHORT_NAME" directory.
    boost::filesystem::wpath path_to_aimp_plugins_work_directory(kPLUGIN_SHORT_NAME);

    // get IAIMP2Extended interface.
    IAIMP2Extended* extended = NULL;
    if ( aimp_controller_->AIMP_QueryObject(IAIMP2ExtendedID, &extended) ) {
        boost::shared_ptr<IAIMP2Extended> aimp_extended( extended, AIMPPlayer::IReleaseFunctor() );
        extended = NULL;

        WCHAR buffer[MAX_PATH];
        const int buffer_length = aimp_extended->AIMP_GetPath(AIMP_CFG_DATA, buffer, MAX_PATH);

        if (0 < buffer_length && buffer_length <= MAX_PATH) {
            path_to_aimp_plugins_work_directory.clear();
            path_to_aimp_plugins_work_directory.append(buffer, buffer + buffer_length);
            path_to_aimp_plugins_work_directory /= kPLUGIN_SHORT_NAME;
        } else {
            // Do nothing here because of logger is not initialized at this point. Default path will be returned.
            //BOOST_LOG_SEV(logger(), error) << "Failed to get path to plugin configuration directory. AIMP_GetPath() returned " << buffer_length;
        }
    }

    return path_to_aimp_plugins_work_directory;
}

void AIMPControlPluginHeader::ensureWorkDirectoryExists()
{
    namespace fs = boost::filesystem;
    plugin_work_directory_ = makePluginWorkDirectory();
    /*  Logic is simple: we try to create plugin_work_directory instead of check if directory existing.
        fs::create_directory() will return false without exception if directory already exists or it returns true if directory is created successfully.
        We avoid code of existing check because of we need to create plugin work directory any way. */
    try {
        fs::create_directory(plugin_work_directory_); // try to create directory unconditionally.
    } catch (fs::basic_filesystem_error<fs::wpath>& e) {
        using namespace StringEncoding;
        // work directory can not be created.
        // TODO: send log to aimp internal logger.
        BOOST_LOG_SEV(logger(), error) << "Failed to create plugin work directory \""
                                       << utf16_to_system_ansi_encoding_safe( plugin_work_directory_.directory_string() )
                                       << "\". Reason:"
                                       << e.what();
    }
}

void AIMPControlPluginHeader::loadSettings()
{
    // Note: logger is not available at this point.

    namespace fs = boost::filesystem;
    const fs::wpath settings_filepath = getSettingsFilePath();

    if ( fs::exists(settings_filepath) ) {
        // load settings from file.
        try {
            settings_manager_.load(settings_filepath);
        } catch (std::exception&) {
            // settings file reading failed. Default settings will be used.
        }
    } else {
        try {
            // save the default settings file.
            settings_manager_.save(settings_filepath);
        } catch (std::exception&) {
            // settings file saving failed.
            // TODO: send to internal AIMP log.
        }
    }
}

void AIMPControlPluginHeader::initializeLogger()
{
    if (settings_manager_.settings().logger.severity_level < PluginLogger::severity_levels_count) {
        plugin_logger_.setSeverity(settings_manager_.settings().logger.severity_level);
        // get absolute path to log directory.
        boost::filesystem::wpath log_directory = settings_manager_.settings().logger.directory;
        if ( !log_directory.is_complete() ) {
            log_directory = plugin_work_directory_ / log_directory;
        }

        try {
            plugin_logger_.startLog(log_directory,
                                    settings_manager_.settings().logger.modules_to_log
                                   );
        } catch (FileLogError& e) {
            // file log is impossible.
            assert(!"File log was not initializated.");
            // Send msg to other log backends.
            BOOST_LOG_SEV(logger(), error)  << "File log was not initializated, "
                                            << "log directory "
                                            << StringEncoding::utf16_to_system_ansi_encoding_safe( log_directory.directory_string() )
                                            << ". Reason: "
                                            << e.what();
        }
    }
}

void WINAPI AIMPControlPluginHeader::Initialize(AIMP2SDK::IAIMP2Controller* AController)
{
    aimp_controller_.reset( AController, AIMPPlayer::IReleaseFunctor() );

    ensureWorkDirectoryExists();
    loadSettings(); // If file does not exist tries to save default settings.
    initializeLogger();

    BOOST_LOG_SEV(logger(), info) << "Plugin initialization is started";

    checkFreeImageDLLAvailability();

    // create plugin core
    try {
        // create AIMP manager.
        aimp_manager_.reset( new AIMPPlayer::AIMPManager(aimp_controller_, server_io_service_) );

        BOOST_LOG_SEV(logger(), info) << "AIMP version: " << aimp_manager_->getAIMPVersion();

        // create multi user mode manager.
        multi_user_mode_manager_.reset( new MultiUserMode::MultiUserModeManager(*aimp_manager_) );

        // create RPC request handler.
        rpc_request_handler_.reset( new Rpc::RequestHandler() );
        createRpcFrontends();
        createRpcMethods();

        using namespace StringEncoding;
        // create HTTP request handler.
        http_request_handler_.reset( new Http::RequestHandler( utf16_to_system_ansi_encoding( getWebServerDocumentRoot().directory_string() ),
                                                               *rpc_request_handler_
                                                              )
                                    );
        // create XMLRPC server.
        server_.reset(new Http::Server( server_io_service_,
                                        settings_manager_.settings().http_server.ip_to_bind,
                                        settings_manager_.settings().http_server.port,
                                        *http_request_handler_
                                       )
                      );

        // start service in separate thread.
        server_thread_.reset( new boost::thread( boost::bind(&AIMPControlPluginHeader::serverThread, this) ) );
    } catch (boost::thread_resource_error& e) {
        BOOST_LOG_SEV(logger(), critical) << "Plugin initialization failed. Reason: create main server thread failed. Reason: " << e.what();
    } catch (std::runtime_error& e) {
        BOOST_LOG_SEV(logger(), critical) << "Plugin initialization failed. Reason: " << e.what();
    } catch (...) {
        BOOST_LOG_SEV(logger(), critical) << "Plugin initialization failed. Reason is unknown";
    }

    BOOST_LOG_SEV(logger(), info) << "Plugin initialization is finished";
}

void WINAPI AIMPControlPluginHeader::Finalize()
{
    BOOST_LOG_SEV(logger(), info) << "Plugin finalization is started";

    server_io_service_.stop();
    
    if (server_thread_) {
        // wait for thread termintion.
        server_thread_->join();

        // destroy server thread
        server_thread_.reset();
    }

    if (server_) {
        // stop the server.
        BOOST_LOG_SEV(logger(), info) << "Stopping server.";

        // destroy the server.
        server_.reset();
    }

    http_request_handler_.reset();

    rpc_request_handler_.reset();

    multi_user_mode_manager_.reset();

    aimp_manager_.reset();

    aimp_controller_.reset();

    BOOST_LOG_SEV(logger(), info) << "Plugin finalization is finished";

    plugin_logger_.stopLog();
}

void WINAPI AIMPControlPluginHeader::ShowSettingsDialog(HWND AParentWindow)
{
    std::wostringstream message_body;
    message_body << L"AIMP2 Control plugin settings can be found in configuration file " << getSettingsFilePath();
    MessageBox( AParentWindow,
                message_body.str().c_str(),
                L"Information about AIMP2 Control Plugin",
                MB_ICONINFORMATION);
}

void AIMPControlPluginHeader::createRpcFrontends()
{
#define REGISTER_RPC_FRONTEND(name) rpc_request_handler_->addFrontend( std::auto_ptr<Rpc::Frontend>( \
                                                                                                    new name::Frontend() \
                                                                                                    )\
                                                                      )
    REGISTER_RPC_FRONTEND(XmlRpc);
    REGISTER_RPC_FRONTEND(JsonRpc);
    REGISTER_RPC_FRONTEND(WebCtlRpc);
#undef REGISTER_RPC_FRONTEND
}

void AIMPControlPluginHeader::createRpcMethods()
{
    using namespace AimpRpcMethods;

#define REGISTER_AIMP_RPC_METHOD(method_type) \
            rpc_request_handler_->addMethod( \
                            std::auto_ptr<Rpc::Method>( \
                                                        new method_type(*aimp_manager_, \
                                                                        *multi_user_mode_manager_, \
                                                                        *rpc_request_handler_\
                                                                        ) \
                                                       ) \
                                            )
    // control panel
    REGISTER_AIMP_RPC_METHOD(Play);
    REGISTER_AIMP_RPC_METHOD(Pause);
    REGISTER_AIMP_RPC_METHOD(Stop);
    REGISTER_AIMP_RPC_METHOD(PlayPrevious);
    REGISTER_AIMP_RPC_METHOD(PlayNext);
    REGISTER_AIMP_RPC_METHOD(ShufflePlaybackMode);
    REGISTER_AIMP_RPC_METHOD(RepeatPlaybackMode);
    REGISTER_AIMP_RPC_METHOD(VolumeLevel);
    REGISTER_AIMP_RPC_METHOD(Mute);
    REGISTER_AIMP_RPC_METHOD(Status);
    REGISTER_AIMP_RPC_METHOD(GetPlayerControlPanelState);
    // playlists
    REGISTER_AIMP_RPC_METHOD(GetPlaylists);
    // tracks
    REGISTER_AIMP_RPC_METHOD(GetPlaylistEntries);

    { // register this way since GetEntryPositionInDataTable depends from GetPlaylistEntries 
    std::auto_ptr<GetPlaylistEntries> method_getplaylistentries(new GetPlaylistEntries(*aimp_manager_,
                                                                                       *multi_user_mode_manager_,
                                                                                       *rpc_request_handler_
                                                                                       )
                                                                );

    std::auto_ptr<Rpc::Method> method_GetEntryPositionInDataTable(new GetEntryPositionInDataTable(*aimp_manager_,
                                                                                     *multi_user_mode_manager_,
                                                                                     *rpc_request_handler_,
                                                                                     *method_getplaylistentries
                                                                                     )
                                                         );
    { // auto_ptr can not be implicitly casted to ptr to object of base class.
    std::auto_ptr<Rpc::Method> method( method_getplaylistentries.release() );
    rpc_request_handler_->addMethod(method);
    }
    rpc_request_handler_->addMethod(method_GetEntryPositionInDataTable);
    }

    REGISTER_AIMP_RPC_METHOD(GetPlaylistEntriesCount);
    REGISTER_AIMP_RPC_METHOD(GetFormattedEntryTitle);
    REGISTER_AIMP_RPC_METHOD(GetPlaylistEntryInfo);
    // track's album cover.
    if (free_image_dll_is_available_) {
        // add document root and path to directory for storing album covers in GetCover method.
        rpc_request_handler_->addMethod( std::auto_ptr<Rpc::Method>(
                                                new GetCover(*aimp_manager_,
                                                             *multi_user_mode_manager_,
                                                             *rpc_request_handler_,
                                                             getWebServerDocumentRoot().directory_string(),
                                                             L"tmp" // directory in document root to store temp image files.
                                                             )
                                                                    )
                                        );
    } else {
        BOOST_LOG_SEV(logger(), info) << "Album cover processing was disabled.";
    }
    // Comet technique, "subscribe" method.
    REGISTER_AIMP_RPC_METHOD(SubscribeOnAIMPStateUpdateEvent);
    // add file name for rating store file to SetTrackRating() method.
    rpc_request_handler_->addMethod( std::auto_ptr<Rpc::Method>(
                                                    new SetTrackRating( *aimp_manager_,
                                                                        *multi_user_mode_manager_,
                                                                        *rpc_request_handler_,
                                                                        (plugin_work_directory_ / L"rating_store.txt").file_string()
                                                                       )
                                                                )
                                    );
    // Emulator of Aimp WebCtl plugin.
    REGISTER_AIMP_RPC_METHOD(EmulationOfWebCtlPlugin);
#undef REGISTER_AIMP_RPC_METHOD
}

LONG WINAPI DelayLoadDllExceptionFilter(PEXCEPTION_POINTERS pep)
{
    // Assume we recognize this exception
    LONG lDisposition = EXCEPTION_EXECUTE_HANDLER;

    // If this is a Delay-load problem, ExceptionInformation[0] points
    // to a DelayLoadInfo structure that has detailed error info
    PDelayLoadInfo pdli = PDelayLoadInfo(pep->ExceptionRecord->ExceptionInformation[0]);

    switch (pep->ExceptionRecord->ExceptionCode)
    {
    case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
        // The DLL module was not found at runtime
        BOOST_LOG_SEV(logger(), warning) << "Dll " << pdli->szDll << " not found.";
        break;

    case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
        // The DLL module was found, but it doesn't contain the function
        if (pdli->dlp.fImportByName) {
            BOOST_LOG_SEV(logger(), warning) << "Function " << pdli->dlp.szProcName << " was not found in " << pdli->szDll;
        } else {
            BOOST_LOG_SEV(logger(), warning) << "Function ordinal " << pdli->dlp.dwOrdinal << " was not found in " << pdli->szDll;
        }
        break;

    default:
        // We don't recognize this exception
        lDisposition = EXCEPTION_CONTINUE_SEARCH;
        break;
    }

    return lDisposition;
}

void freeImagePlusDllTest()
{
    fipWinImage img; // check fipWinImage::fipWinImage() availability.
    img.copyFromBitmap(0); // check fipWinImage::copyFromBitmap() availability.
    FreeImageIO io;
    img.saveToHandle(FIF_PNG, &io, NULL); // check fipWinImage::saveToHandle() availability.
}

void AIMPControlPluginHeader::checkFreeImageDLLAvailability()
{
    // Wrap all calls to delay-load DLL functions inside SEH
    __try {
        freeImagePlusDllTest();
        free_image_dll_is_available_ = true; // if we are here - FreeImagePlus dll was loaded fine, allow using.
    } __except ( DelayLoadDllExceptionFilter( GetExceptionInformation() ) ) {
        // DLL was not loaded, using of FreeImagePlus functions is forbidden.
    }
}

boost::filesystem::wpath AIMPControlPluginHeader::getWebServerDocumentRoot() const // throws std::runtime_error
{
    // get document root from settings.
    namespace fs = boost::filesystem;
    fs::wpath document_root_path = settings_manager_.settings().http_server.document_root;
    if ( !document_root_path.is_complete() ) {
        document_root_path = plugin_work_directory_ / document_root_path;
    }

    if ( !( fs::exists(document_root_path) && fs::is_directory(document_root_path) ) ) {
        std::ostringstream os;
        os << "Web-server document root directory does not exist: \"" << StringEncoding::utf16_to_system_ansi_encoding( document_root_path.string() ) << "\"";
        throw std::runtime_error( os.str() );
    }

    return document_root_path;
}

void AIMPControlPluginHeader::serverThread()
{
    BOOST_LOG_SEV(logger(), info) << "Start service.";

    for (;;) {
        try {
            server_io_service_.run();
            break; // run() exited normally
        } catch (std::exception& e) {
            // Just send error in log and exit.
            BOOST_LOG_SEV(logger(), critical) << "Unhandled exception inside http server thread: " << e.what();
            server_io_service_.stop();
            break;
        }
    }

    BOOST_LOG_SEV(logger(), info) << "Service was stopped.";
}

} // namespace AIMPControlPlugin
