#pragma once
#include "sqlite_wrapper.h"

#include <memory>

namespace sql
{
    class database
    {
    private:
        std::string _path = "";
        HANDLE _mutex = NULL;
        std::unique_ptr<sql::sqlite> _db;

        void lock_db();
        void unlock_db();

    public:
        // Open latest database
        database(const std::string& db_dir, bool read_only = true);
        ~database();

        // Do query and return list of records
        // Throw exception on any errors.
        std::list<record> search(const std::string& statement);

        // Do insert to database
        // Throw exception on any errors.
        // Update to last database file, filename = database_YYYYmm.db
        void update(const std::list<std::string>& statements);
    };
}