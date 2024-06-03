#include "sqlite_wrapper.h"

#include <vector>

namespace sql
{
    // Class sqlite

    int sqlite::step_and_finalize(sqlite3_stmt* stmt)
    {
        int ret = sqlite3_step(stmt);
        int finalize_ret = sqlite3_finalize(stmt);
        return ret == SQLITE_DONE ? finalize_ret : ret;
    }

    sqlite::sqlite(const std::string& db_name)
    {
        if (sqlite3_open(db_name.c_str(), &_db) != 0)
            throw std::runtime_error("cannot open db");
    }

    sqlite::~sqlite()
    {
        sqlite3_close(_db);
    }

    void sqlite::exec(const std::string& statement)
    {
        char* errmsg = nullptr;
        int err = sqlite3_exec(_db, statement.c_str(), NULL, 0, &errmsg);
        if (errmsg != nullptr)
            sqlite3_free(errmsg);
        if (err != SQLITE_OK)
            throw std::runtime_error("sqlite::exec failed");
    }

    void sqlite::begin_transaction()
    {
        int err = sqlite3_exec(_db, "BEGIN TRANSACTION", NULL, NULL, NULL);
        if (err != SQLITE_OK)
            throw std::runtime_error(nstd::format("%s() sqlite3_exec failed with error: %d", __FUNCTION__, err).c_str());
    }

    void sqlite::commit_transaction()
    {
        int err = sqlite3_exec(_db, "COMMIT TRANSACTION", NULL, NULL, NULL);
        if (err != SQLITE_OK)
            throw std::runtime_error(nstd::format("sqlite3_exec failed with error: %d", err));
    }

    std::list<record> sqlite::search(const std::string& statement)
    {
        sqlite3_stmt* stmt = NULL;
        int err;
        if ((err = sqlite3_prepare_v2(_db, statement.c_str(), -1, &stmt, NULL)) != SQLITE_OK)
            throw std::runtime_error(nstd::format("%s() sqlite3_prepare_v2 failed with error: %d", __FUNCTION__, err).c_str());
        defer{ step_and_finalize(stmt); };

        std::list<record> results;
        int num_cols = sqlite3_column_count(stmt);
        std::vector<std::string> column_names;
        for (int i = 0; i < num_cols; ++i)
        {
            const char* colname = sqlite3_column_name(stmt, i);
            column_names.push_back(colname ? colname : "");
        }

        int ret;
        while ((ret = sqlite3_step(stmt)) != SQLITE_DONE)
        {
            record row;
            for (int i = 0; i < num_cols; ++i)
            {
                switch (sqlite3_column_type(stmt, i))
                {
                case SQLITE3_TEXT:
                {
                    const unsigned char* value = sqlite3_column_text(stmt, i);
                    int len = sqlite3_column_bytes(stmt, i);
                    row[column_names[i]] = std::string(value, value + len);
                    break;
                }
                case SQLITE_INTEGER:
                {
                    row[column_names[i]] = nstd::format("%d", sqlite3_column_int(stmt, i));
                    break;
                }
                case SQLITE_FLOAT:
                {
                    row[column_names[i]] = nstd::format("%f", sqlite3_column_double(stmt, i));
                    break;
                }
                default:
                    break;
                }

            }
            results.push_back(row);
        }
        return results;
    }

    /*void sqlite::exec(const std::string& statement)
    {
        sqlite3_stmt* stmt = NULL;
        int err = sqlite3_prepare_v2(_db, statement.c_str(), -1, &stmt, NULL);
        if (err != SQLITE_OK)
            throw std::runtime_error(nstd::format("%s() sqlite3_prepare_v2 failed with error: %d", __FUNCTION__, err).c_str());

        err = step_and_finalize(stmt);
        if (err != SQLITE_OK)
            throw std::runtime_error(nstd::format("%s() step_and_finalize failed with error: %d", __FUNCTION__, err).c_str());
    }*/
}