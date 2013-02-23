#pragma once

// This transaltion unit must not include sqlite3.h to avoid unresolved symbols due to they declared as external in "sqlite3.h".
//#include "sqlite3/sqlite3.h" 

// Use dynamic linking with sqlite3.dll in AIMP root dir.
//#define _HAVE_SQLITE_CONFIG_H
//#include "sqlite3/sqlite3.c"