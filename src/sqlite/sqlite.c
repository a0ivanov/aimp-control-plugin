#pragma once

// This transaltion unit must not include sqlite3.h to avoid unresolved symbols due to they declared as external in "sqlite3.h".
// #include "sqlite3.h" 
// #include "sqlite3.c"

#include "sqlite.h"

#define SQLITE_ENABLE_UNICODE
#include "sqlite3_unicode.c"
