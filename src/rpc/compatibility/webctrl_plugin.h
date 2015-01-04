#include "../methods.h"

namespace AimpRpcMethods
{

class EmulationOfWebCtlPlugin : public AIMPRPCMethod
{
public:
    EmulationOfWebCtlPlugin(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("EmulationOfWebCtlPlugin", aimp_manager, rpc_request_handler)
    {
        initMethodNamesMap();
    }

    std::string help()
    {
        return "see documentation of aimp-web-ctl plugin";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    enum METHOD_ID {
        get_playlist_list,
        get_version_string,
        get_version_number,
        get_update_time,
        get_playlist_songs,
        get_playlist_crc,
        get_player_status,
        get_song_current,
        get_volume,
        set_volume,
        get_track_position,
        set_track_position,
        get_track_length,
        get_custom_status,
        set_custom_status,
        set_song_play,
        set_song_position,
        set_player_status,
        player_play,
        player_pause,
        player_stop,
        player_prevous,
        player_next,
        playlist_sort,
        playlist_add_file,
        playlist_del_file,
        playlist_queue_add,
        playlist_queue_remove,
        download_song
    };

    typedef std::map<std::string, METHOD_ID> MethodNamesMap;

    void initMethodNamesMap();

    const METHOD_ID* getMethodID(const std::string& method_name) const;

    void getPlaylistList(std::ostringstream& out);
    void getPlaylistSongs(int playlist_id, bool ignore_cache, bool return_crc, int offset, int size, std::ostringstream& out);
    void getPlayerStatus(std::ostringstream& out);
    void getCurrentSong(std::ostringstream& out);
    void calcPlaylistCRC(int playlist_id);
    void setPlayerStatus(const std::string& statusType, int value);
    void sortPlaylist(int playlist_id, const std::string& sortType);
    void addFile(int playlist_id, const std::string& filename_url);


    MethodNamesMap method_names_;
};

} // namespace AimpRpcMethods
