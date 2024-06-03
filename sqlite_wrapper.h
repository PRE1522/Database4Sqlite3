#pragma once
#include "stdafx.h"

#include <list>
#include <map>
#include <string>

#include "sqlite3.h"

namespace sql
{
    using record = std::map<std::string, std::string>;

    class sqlite
    {
    private:
        sqlite3* _db = nullptr;

        static int step_and_finalize(sqlite3_stmt* stmt);

    public:
        // Open sqlite file to read or write
        // Throws exception if any error occurs.
        sqlite(const std::string& db_name);
        ~sqlite();

        // Exec SQL query
        void exec(const std::string& stmt);

        // Begin SQL transaction.
        void begin_transaction();

        // Commit SQL transaction.
        void commit_transaction();

        // Do query and return list of records
        // Throw exception on any errors.
        std::list<record> search(const std::string& statement);

        // Do insert/update to database
        // Throw exception on any errors.
        void update(const std::string& statement);
    };
}