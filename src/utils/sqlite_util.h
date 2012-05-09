#pragma once

#include "sqlite/sqlite3.h"
#include "util.h"
#include "scope_guard.h"
#include <boost/function.hpp>
#include <list>

namespace Utilities {

// Note: you should call sqlite3_finalize() after work is done.
inline sqlite3_stmt* CreateStmt(sqlite3* db, const std::string& query) // throws std::runtime_error
{
    sqlite3_stmt* stmt = nullptr;
    int rc_db = sqlite3_prepare( db,
                                 query.c_str(),
                                 -1, // If less than zero, then stmt is read up to the first nul terminator
                                 &stmt,
                                 nullptr  // Pointer to unused portion of stmt
                                );
    if (SQLITE_OK != rc_db) {
        using namespace Utilities;
        const std::string msg = MakeString() << "sqlite3_prepare() error "
                                             << rc_db << ": " << sqlite3_errmsg(db)
                                             << ". Query: " << query;
        throw std::runtime_error(msg);
    }
    return stmt;
}

typedef boost::function<void(sqlite3_stmt*, int)> QueryArgSetter;
typedef std::list<QueryArgSetter> QueryArgSetters;

inline size_t GetRowsCount(sqlite3* db, const std::string& query, const QueryArgSetters* query_arg_setters = nullptr)
{
    sqlite3_stmt* stmt = CreateStmt(db, query);
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    if (query_arg_setters) { // bind all query args.
        size_t bind_index = 1;
        BOOST_FOREACH(auto& setter, *query_arg_setters) {
            setter(stmt, bind_index++);
        }
    }

    size_t entries_count = 0;
    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
			++entries_count;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query;
            throw std::runtime_error(msg);
		}
    }

    return entries_count;
}

} // namespace Utilities
