// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "aimp/aimp2_sdk.h"
#include "aimp/aimp3_sdk/aimp3_sdk.h"
#include "aimp/aimp3.60_sdk/aimp3_60_sdk.h"
#include "settings.h"
#include "logger.h"
#include "utils/iunknown_impl.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace Http          { class RequestHandler; }
namespace Rpc           { class RequestHandler; }
namespace DownloadTrack { class RequestHandler; }
namespace UploadTrack   { class RequestHandler; }
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

    // API 3.0
    HRESULT Initialize(AIMP3SDK::IAIMPCoreUnit* ACoreUnit);
    HRESULT Finalize();
    HRESULT ShowSettingsDialog(HWND AParentWindow);
    
    // API 3.60 begin
    HRESULT Initialize(AIMP36SDK::IAIMPCore* Core);
    
    PWCHAR InfoGet(int index);

    DWORD InfoGetCategories() 
        { return AIMP36SDK::AIMP_PLUGIN_CATEGORY_ADDONS; }

    void SystemNotification(int notifyID, IUnknown* data);
    // API 3.60 end

    // Objects returned by following functions are valid between Initialize and Finalize() methods calls.
    //! Returns global reference to plugin logger object.(Singleton pattern)
    static PluginLogger::LogManager& getLogManager();

    //! Returns global reference to settings object.
    static const PluginSettings::Settings& settings();

    static boost::filesystem::wpath getPluginDirectoryPath();

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
    std::wstring getAimpPluginsPath() const;
    std::wstring getAimpPath(int path_id) const; // helper for implementation of getAimpProfilePath/getAimpPluginsPath methods.

    boost::filesystem::wpath getPluginDirectoryPath(const boost::filesystem::wpath& base_directory) const;

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
    boost::filesystem::wpath getSettingsFilePath(const boost::filesystem::wpath& plugin_work_directory) const;

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

    boost::intrusive_ptr<AIMP36SDK::IAIMPCore> aimp36_core_;

    boost::shared_ptr<AIMPPlayer::AIMPManager> aimp_manager_; //!< AIMP player manager.

    boost::filesystem::wpath plugin_work_directory_; //!< path to plugin work directory. It is initialized in getPluginWorkDirectoryPathInAimpPluginsDirectory() function.
                       
    // plugin settings.
    boost::filesystem::wpath plugin_settings_filepath_;

    boost::shared_ptr<Rpc::RequestHandler> rpc_request_handler_; //!< XML/Json RPC request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<DownloadTrack::RequestHandler> download_track_request_handler_; //!< Download track request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<UploadTrack::RequestHandler> upload_track_request_handler_; //!< Upload track request handler. Used by Http::RequestHandler object.
    boost::shared_ptr<Http::RequestHandler> http_request_handler_; //!< Http request handler, used by Http::Server object.
    boost::shared_ptr<boost::asio::io_service> server_io_service_;
    boost::shared_ptr<Http::Server> server_; //!< Simple Http server.

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

class AIMP36ControlPlugin : public IUnknownInterfaceImpl<AIMP36SDK::IAIMPPlugin>, public AIMP36SDK::IAIMPExternalSettingsDialog
{
public:

		// Information about the plugin
        virtual PWCHAR WINAPI InfoGet(int Index) {
            return plugin.InfoGet(Index);
        }
        virtual DWORD WINAPI InfoGetCategories() {
            return plugin.InfoGetCategories();
        }
		// Initialization / Finalization
        virtual HRESULT WINAPI Initialize(AIMP36SDK::IAIMPCore* Core) {
            return plugin.Initialize(Core);
        }
		virtual HRESULT WINAPI Finalize() {
            return plugin.Finalize();
        }
		// System Notifications
        virtual void WINAPI SystemNotification(int NotifyID, IUnknown* Data) {
            plugin.SystemNotification(NotifyID, Data);
        }

        // AIMPExternalSettingsDialog interface
        virtual void WINAPI Show(HWND ParentWindow) {
            plugin.ShowSettingsDialog(ParentWindow);
        }

        virtual HRESULT WINAPI QueryInterface(REFIID riid, LPVOID* ppvObj) {
            if (!ppvObj) {
                return E_POINTER;
            }

            if (IID_IUnknown == riid) {
                *ppvObj = this;
                AddRef();
                return S_OK;
            } else if (AIMP36SDK::IID_IAIMPExternalSettingsDialog == riid) {
                *ppvObj = static_cast<AIMP36SDK::IAIMPExternalSettingsDialog*>(this);
                AddRef();
                return S_OK;                
            }

            return E_NOINTERFACE;
        }

        virtual ULONG WINAPI AddRef(void)
            { return Base::AddRef(); }

        virtual ULONG WINAPI Release(void)
            { return Base::Release(); }
            
private:

    typedef IUnknownInterfaceImpl<AIMP36SDK::IAIMPPlugin> Base;
    AIMPControlPlugin plugin;
};

} // namespace ControlPlugin
