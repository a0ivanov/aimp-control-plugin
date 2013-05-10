#pragma once

// Aimp2 does not notify plugins about playlist content changes, so we check it manually.
#define MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

// Turn off SQLite thread syncronization since use use only one thread.
#define SQLITE_THREADSAFE (0)
