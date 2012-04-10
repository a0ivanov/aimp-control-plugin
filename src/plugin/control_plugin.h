// Copyright (c) 2012, Alexey Ivanov

#ifndef AIMP_CONTROL_PLUGIN_H
#define AIMP_CONTROL_PLUGIN_H

#include "aimp/aimp2_sdk.h"
#include "aimp/aimp3_sdk/aimp3_sdk.h"
#include "settings.h"
#include "logger.h"
#include "utils/iunknown_impl.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace Http { class RequestHandler; }
namespace Rpc { class RequestHandler; }
namespace DownloadTrack { class RequestHandler; }
namespace AIMP2SDK { class IAIMP2Controller; }

//! contains class which implements AIMP SDK interfaces and interacts with AIMP player.
namespace ControlPlugin
{

/*!
    Manages all objects which are doing real job.
*/
class AIMPControlPlugin
{
public:

    static const std::wstring kPLUGIN_AUTHOR;     //!< Plugin author name which is displayed by AIMP player in plugin 'About' field.
    static const std::wstring kPLUGIN_INFO;       //!< Plugin info which is displayed by AIMP player in plugin 'About' field.
    static const std::wstring kPLUGIN_SHORT_NAME; //!< Plugin name which is displayed by AIMP player in plugin 'About' field.

    AIMPControlPlugin();
    ~AIMPControlPlugin();

    void Initialize(AIMP2SDK::IAIMP2Controller* AController);
    HRESULT Initialize(AIMP3SDK::IAIMPCoreUnit* ACoreUnit);
    HRESULT Finalize();
    HRESULT ShowSettingsDialog(HWND AParentWindow);
    
    //! Returns global reference to plugin logger object.(Singleton pattern)
    static PluginLogger::LogManager& getLogManager();

private:

    HRESULT initialize();

    // Runs the server's io_service loop.
    void onTick();

    static void CALLBACK onTickTimerProc(HWND hwnd,
                                         UINT uMsg,
                                         UINT_PTR idEvent,
                                         DWORD dwTime);

    void startTickTimer();
    void stopTickTimer();

    /*! 
        Extracts path of AIMP Profile directory. 
            In portable mode:
                Profile directory near AIMP3.exe file.
            In multiuser mode:
                AIMP3:
                    %APPDATA%\AIMP3
                AIMP2:
                    %APPDATA%\AIMP
        \return full path to aimp profile directory or (in case of error) current directory "".
    */
    std::wstring getAimpProfilePath();

    /*! 
        Extracts path of Plugins directory. Usually it's Plugins directory near AIMP3.exe file.
        \return full path to plugins directory or (in case of error) current directory "".
    */
    std::wstring getAimpPluginsPath();
    std::wstring getAimpPath(int path_id); // helper for implementation of getAimpProfilePath/getAimpPluginsPath methods.

    boost::filesystem::wpath getPluginWorkDirectoryPath();
    boost::filesystem::wpath getPluginWorkDirectoryPath(const boost::filesystem::wpath& base_directory) const;

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
    boost::filesystem::wpath getSettingsFilePath() const;
    boost::filesystem::wpath getSettingsFilePath(const boost::filesystem::wpath& plugin_work_directory) const;
    boost::filesystem::wpath getSettingsFilePathVersion1_0_7_825_and_older();

    boost::shared_ptr<AIMPPlayer::AIMPManager> CreateAIMPManager(); // throws std::runtime_error

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
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp2_controller_;

    /*!
        \brief internal AIMP core object, passed in plugin through AIMP3SDK::IAIMPAddon::Initialize function by AIMP player.
        Used mostly by AIMPPlayer::AIMPManager object.
    */
    boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit_;

    boost::shared_ptr<Rpc::RequestHandler> rpc_request_handler_; //!< XML/Json RPC request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<DownloadTrack::RequestHandler> download_track_request_handler_; //!< Download track request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<Http::RequestHandler> http_request_handler_; //!< Http request handler, used by Http::Server object.
    boost::shared_ptr<boost::asio::io_service> server_io_service_;
    boost::shared_ptr<Http::Server> server_; //!< Simple Http server.

    boost::shared_ptr<AIMPPlayer::AIMPManager> aimp_manager_; //!< AIMP player manager.

    // plugin settings.
    boost::filesystem::wpath plugin_work_directory_; //!< stores path to plugin work directory. It is initialized in getPluginWorkDirectoryPath() function.

    static const std::wstring kPLUGIN_SETTINGS_FILENAME; //<! default plugin settings filename.

    PluginSettings::Manager settings_manager_; //< plugin settings manager. Load/save settings to file.

    UINT_PTR tick_timer_id_;
};


/*!
    \brief provides implementation AIMP2SDK::IAIMPAddonHeader interface.
*/
class AIMP2ControlPlugin : public IUnknownInterfaceImpl<AIMP2SDK::IAIMPAddonHeader>
{
public:

    //@{
    //! Implementation of AIMP2SDK::IAIMPAddonHeader interface.
    virtual BOOL WINAPI GetHasSettingsDialog() { 
        return TRUE;
    }
    virtual PWCHAR WINAPI GetPluginAuthor() {
        return const_cast<PWCHAR>( AIMPControlPlugin::kPLUGIN_AUTHOR.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
    }
    virtual PWCHAR WINAPI GetPluginName() {
        return const_cast<const PWCHAR>( AIMPControlPlugin::kPLUGIN_SHORT_NAME.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
    }
    virtual void WINAPI Initialize(AIMP2SDK::IAIMP2Controller* AController) { 
        plugin.Initialize(AController);
    }
    virtual void WINAPI Finalize() { 
        plugin.Finalize();
    }
    virtual void WINAPI ShowSettingsDialog(HWND AParentWindow) {
        plugin.ShowSettingsDialog(AParentWindow);
    }
    //@}

private:

    AIMPControlPlugin plugin;
};

/*!
    \brief provides implementation AIMP3SDK::IAIMPAddonPlugin interface.
*/
class AIMP3ControlPlugin : public IUnknownInterfaceImpl<AIMP3SDK::IAIMPAddonPlugin>
{
public:

    //@{
    //! Implementation of AIMP3SDK::IAIMPAddonPlugin interface.
    virtual PWCHAR WINAPI GetPluginAuthor() {
        return const_cast<PWCHAR>( AIMPControlPlugin::kPLUGIN_AUTHOR.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
    }
    virtual PWCHAR WINAPI GetPluginInfo() {
        return const_cast<PWCHAR>( AIMPControlPlugin::kPLUGIN_INFO.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
    }
    virtual PWCHAR WINAPI GetPluginName() {
        return const_cast<const PWCHAR>( AIMPControlPlugin::kPLUGIN_SHORT_NAME.c_str() ); // const cast is safe here since AIMP does not try to modify these data.
    }
    virtual DWORD WINAPI GetPluginFlags() { 
        return AIMP3SDK::AIMP_ADDON_FLAGS_HAS_DIALOG;
    }
    virtual HRESULT WINAPI Initialize(AIMP3SDK::IAIMPCoreUnit* ACoreUnit) {
        return plugin.Initialize(ACoreUnit);
    }
    virtual HRESULT WINAPI Finalize() {
        return plugin.Finalize();
    }
    virtual HRESULT WINAPI ShowSettingsDialog(HWND AParentWindow) { 
        return plugin.ShowSettingsDialog(AParentWindow);
    }
    //@}

private:

    AIMPControlPlugin plugin;
};

} // namespace ControlPlugin

#endif // #ifndef AIMP_CONTROL_PLUGIN_H
