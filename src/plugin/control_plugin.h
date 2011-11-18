// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_CONTROL_PLUGIN_H
#define AIMP_CONTROL_PLUGIN_H

#include "aimp/aimp2_sdk.h"
#include "aimp/aimp3_sdk/aimp3_sdk.h"
#include "settings.h"
#include "logger.h"
#include "utils/iunknown_impl.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace MultiUserMode { class MultiUserModeManager; }
namespace Http { class RequestHandler; }
namespace Rpc { class RequestHandler; }
namespace AIMP2SDK { class IAIMP2Controller; }

//! contains class which implements AIMP SDK interfaces and interacts with AIMP player.
namespace AIMPControlPlugin
{

/*!
    \brief provides implementation AIMP2SDK::IAIMPAddonHeader interface.
    Manages all objects which are doing real job.
*/
class AIMP2ControlPlugin : public IUnknownInterfaceImpl<AIMP2SDK::IAIMPAddonHeader>
{
public:
    static const std::wstring kPLUGIN_SHORT_NAME; //!< Plugin name which is displayed by AIMP player in plugin 'About' field.
    static const std::wstring kPLUGIN_AUTHOR; //!< Plugin author name which is displayed by AIMP player in plugin 'About' field.

    AIMP2ControlPlugin();

    //@{
    //! Implementation of AIMP2SDK::IAIMPAddonHeader interface.
    virtual BOOL WINAPI GetHasSettingsDialog();
    virtual PWCHAR WINAPI GetPluginAuthor();
    virtual PWCHAR WINAPI GetPluginName();
    virtual void WINAPI Initialize(AIMP2SDK::IAIMP2Controller* AController);
    virtual void WINAPI Finalize();
    virtual void WINAPI ShowSettingsDialog(HWND AParentWindow);
    //@}

    //! Returns global reference to plugin logger object.(Singleton pattern)
    static PluginLogger::LogManager& getLogManager();

private:

    // Runs the server's io_service loop.
    void OnTick();

    static void CALLBACK OnTickTimerProc(HWND hwnd,
                                         UINT uMsg,
                                         UINT_PTR idEvent,
                                         DWORD dwTime);

    void StartTickTimer();
    void StopTickTimer();

    /*! Asks AIMP about plugins working directory.
        \return full path to plugin work directory or (in case of error) current directory ".\$(short_plugin_name)"..
    */
    boost::filesystem::wpath makePluginWorkDirectory();

    //! Checks existing of work directory and create it if it is neccessary.
    void ensureWorkDirectoryExists();

    /*!
        \brief Loads settings from xml file kPLUGIN_SETTINGS_FILENAME.
               If file does not exist tries to save default settings.
    */
    void loadSettings();

    /*!
        \brief Returns path to settings file.
        \return absolute path to settings file.
    */
    boost::filesystem::wpath getSettingsFilePath();

    //! Initializes logger in case if settings log level is not LogManager::NONE.
    void initializeLogger();

    //! Creates and register in Rpc::RequestHandler object all supported frontends.
    void createRpcFrontends();

    //! Creates and register in Rpc::RequestHandler object all RPC methods.
    void createRpcMethods();

    /*!
        \brief Tries to load delayed FreeImage DLL. Set flag that signals that plugin can use FreeImage functionality.
    */
    void checkFreeImageDLLAvailability();

    /*!
        \brief Gets absolute document root directory path from settings.
        \return path relative to plugin work directory if path in settings is not absolute.
        \throw std::runtime_error if path does not exist or it is not a directory.
    */
    boost::filesystem::wpath getWebServerDocumentRoot() const; // throws std::runtime_error

    bool free_image_dll_is_available_; //!< flag is set if FreeImage DLL is accessible. See checkFreeImageDLLAvailability().

    PluginLogger::LogManager plugin_logger_; //!< logger object, accessible through getLogManager().

    /*!
        \brief internal AIMP controller object, passed in plugin through AIMP2SDK::IAIMPAddonHeader::Initialize function by AIMP player.
        Used mostly by AIMPPlayer::AIMPManager object.
    */
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller_;

    boost::shared_ptr<MultiUserMode::MultiUserModeManager> multi_user_mode_manager_; //!< TODO: add doc
    boost::shared_ptr<Rpc::RequestHandler> rpc_request_handler_; //!< XML/Json RPC request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<Http::RequestHandler> http_request_handler_; //!< Http request handler, used by Http::Server object.
    boost::asio::io_service server_io_service_;
    boost::shared_ptr<Http::Server> server_; //!< Simple Http server.

    boost::shared_ptr<AIMPPlayer::AIMPManager> aimp_manager_; //!< AIMP player manager.

    // plugin settings.
    boost::filesystem::wpath plugin_work_directory_; //!< stores path to plugin work directory. It is initialized in makePluginWorkDirectory() function.

    static const std::wstring kPLUGIN_SETTINGS_FILENAME; //<! default plugin settings filename.

    PluginSettings::Manager settings_manager_; //< plugin settings manager. Load/save settings to file.

    UINT_PTR tick_timer_id_;
};


class AIMP3ControlPlugin : public AIMP3SDK::IAIMPAddonPlugin, public AIMP3SDK::IAIMPAddonsFileInfoRepository
{
    static const std::wstring kPLUGIN_AUTHOR;     //!< Plugin author name which is displayed by AIMP player in plugin 'About' field.
    static const std::wstring kPLUGIN_INFO;       //!< Plugin info which is displayed by AIMP player in plugin 'About' field.
    static const std::wstring kPLUGIN_SHORT_NAME; //!< Plugin name which is displayed by AIMP player in plugin 'About' field.

public:

    AIMP3ControlPlugin()
        :
        reference_count_(0)
    {}

    // IUnknown methods.
    virtual HRESULT WINAPI QueryInterface(REFIID riid, LPVOID* ppvObj);
    virtual ULONG WINAPI AddRef(void);
    virtual ULONG WINAPI Release(void);

    // AIMP3SDK::IAIMPAddonPlugin methods.
    virtual PWCHAR WINAPI GetPluginAuthor();
    virtual PWCHAR WINAPI GetPluginInfo();
    virtual PWCHAR WINAPI GetPluginName();
    virtual DWORD WINAPI GetPluginFlags();
    virtual HRESULT WINAPI Initialize(AIMP3SDK::IAIMPCoreUnit* coreUnit);
    virtual HRESULT WINAPI Finalize();
    virtual HRESULT WINAPI ShowSettingsDialog(HWND parentWindow);

    // AIMP3SDK::IAIMPAddonsFileInfoRepository method.
    virtual HRESULT WINAPI GetInfo(PWCHAR AFile, AIMP3SDK::TAIMPFileInfo* AInfo);

private:

    void OnTick();

    static void CALLBACK OnTickTimerProc(HWND hwnd,
                                         UINT uMsg,
                                         UINT_PTR idEvent,
                                         DWORD dwTime);

    void StartTickTimer();
    void StopTickTimer();

    boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp_core_unit_;
    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlayerManager> aimp_player_manager_;
    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistManager> aimp_playlist_manager_;

    ULONG reference_count_;
    UINT_PTR tick_timer_id_;
};

} // namespace AIMPControlPlugin

#endif // #ifndef AIMP_CONTROL_PLUGIN_H
