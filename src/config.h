#ifndef CONFIG_H_
#define CONFIG_H_

// Aimp2 does not notify plugins about playlist content changes, so we check it in our own thread.
// Waiting Aimp3 with new plugin engine to turn it off.
#define MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

#endif // #ifndef CONFIG_H_
