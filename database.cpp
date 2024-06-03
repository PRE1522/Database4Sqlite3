#include "database.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <direct.h>
#include <sstream>
#include <fstream>

#include "Common.h"

#define HASH_MUTEX  L"4EB9644E0AE299BAC5DBCC4440C066E6"

#define DB_VERSION "Thread_hunting_ver1"

namespace sql
{
    static std::list<std::string> list_dir(const std::string& path, bool recursive)
    {
        std::list<std::string> ret;

        // check if path is directory
        DWORD dwAttr = GetFileAttributesA(path.c_str());
        if (dwAttr == INVALID_FILE_ATTRIBUTES ||
            (dwAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return ret;

        WIN32_FIND_DATAA FindFileData;
        std::string mask = (path.back() == '\\') ? path + '*' : path + "\\*";
        HANDLE hFind = FindFirstFileA(mask.c_str(), &FindFileData);
        if (hFind == INVALID_HANDLE_VALUE)
            return ret;
        do
        {
            if (lstrcmpA(FindFileData.cFileName, ".") == 0 || lstrcmpA(FindFileData.cFileName, "..") == 0)
                continue;
            std::string sub_path = (path.back() == '\\') ? path + FindFileData.cFileName : path + "\\" + FindFileData.cFileName;
            if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                ret.push_back(sub_path);

            if (recursive && FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                for (auto &p : list_dir(sub_path, recursive))
                    ret.push_back(p);
            }
        } while (FindNextFileA(hFind, &FindFileData));
        FindClose(hFind);

        return ret;
    }

    static bool is_existed(const std::string& name) {
        std::ifstream f(name.c_str());
        return f.good();
    }

    static bool is_valid_db(sql::sqlite* data_object)
    {
        auto version = data_object->search("SELECT * FROM VERSION");
        auto& x = version.back();
        if (x["version"].find(DB_VERSION) != std::string::npos)
            return true;
        return false;
    }

    static void init_table(sql::sqlite* db_object)
    {
        db_object->begin_transaction();

        // Create table file
        db_object->exec(R"(CREATE TABLE IF NOT EXISTS FILE (id INTEGER PRIMARY KEY NOT NULL, 
            file_name TEXT COLLATE NOCASE,
            file_path TEXT COLLATE NOCASE, 
            file_hash_md5 TEXT COLLATE NOCASE, 
            file_hash_sha256 TEXT COLLATE NOCASE, 
            file_hash_sha1 TEXT COLLATE NOCASE, 
            file_signature TEXT COLLATE NOCASE, 
            created_time INTEGER,
            modified_time INTEGER);)");
        db_object->exec("CREATE INDEX IF NOT EXISTS 'IdxNode_File' ON FILE(file_path, file_name, file_signature);");

        // Create table registry
        db_object->exec("CREATE TABLE IF NOT EXISTS REGISTRY (id INTEGER PRIMARY KEY NOT NULL, reg_path TEXT, reg_key TEXT, reg_value TEXT, created_time INTEGER, modified_time INTEGER, detected_time INTEGER);");
        db_object->exec("CREATE INDEX IF NOT EXISTS 'IdxNode_Registry' ON REGISTRY(reg_path, reg_key);");

        // Create table network
        db_object->exec("CREATE TABLE IF NOT EXISTS NETWORK (id INTEGER PRIMARY KEY NOT NULL, target_ip TEXT, target_domain TEXT, process_id INTEGER, process_path TEXT, connected_time INTEGER);");
        db_object->exec("CREATE INDEX IF NOT EXISTS 'IdxNode_Network' ON NETWORK(target_ip, process_id);");

        // Create table process
        db_object->exec("CREATE TABLE IF NOT EXISTS PROCESS (id INTEGER PRIMARY KEY NOT NULL, parent_process_path TEXT, parent_process_id INTEGER, parent_cmdline TEXT, process_path TEXT, process_id INTEGER, cmdline TEXT, user_name TEXT, integrity TEXT, start_time INTEGER, end_time INTEGER);");
        db_object->exec("CREATE INDEX IF NOT EXISTS 'IdxNode_Process' ON PROCESS(process_path, process_id);");

        //Create table version
        db_object->exec("CREATE TABLE IF NOT EXISTS VERSION (id INTEGER PRIMARY KEY NOT NULL, version TEXT);");
        db_object->exec(nstd::format("INSERT INTO VERSION(version) VALUES('%s');", DB_VERSION));

        db_object->commit_transaction();
    }

    // return formatted date: YYYYmm
    static std::string get_date()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo;
        localtime_s(&timeinfo, &now_time);
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%y%m");
        std::string formatted_date = ss.str();
        return formatted_date;
    }

    void database::lock_db() 
    {
        if (WaitForSingleObject(_mutex, 30 * 1000) == WAIT_TIMEOUT)
            throw std::runtime_error("cannot access to db now");
    }

    void database::unlock_db() 
    {
        if (ReleaseMutex(_mutex) == FALSE) 
            throw std::runtime_error("cannot release mutex");
    }

    database::database(const std::string& db_dir, bool read_only) : 
        _path(db_dir)
    {
        bool success = false;
        _mutex = CreateMutex(NULL, FALSE, HASH_MUTEX);
        if (_mutex == NULL)
            throw std::runtime_error("Unable to access mutex");
        defer{ if (success == false) CloseHandle(_mutex); };

        auto db_path = db_dir + "\\database_" + get_date() + ".db";       
        if (is_existed(db_path) == true)
        {
            try
            {
                _db = std::make_unique<sql::sqlite>(db_path);
                if (is_valid_db(_db.get()) == false)
                    throw std::runtime_error("data corrupted");

                // sqlite file valid
                success = true;
                return;
            }
            catch (std::exception& e)
            {
                auto bak_name = db_path + ".corrupt";
                if (std::rename(db_path.c_str(), bak_name.c_str()) != 0)
                    throw std::runtime_error("rename db file corrupt failed");
            }
        }

        _db = std::make_unique<sql::sqlite>(db_path);
        init_table(_db.get());
        success = true;
    }

    database::~database()
    {
        CloseHandle(_mutex);
    }

    std::list<record> database::search(const std::string& statement)
    {
        lock_db();
        defer{ unlock_db(); };

        std::list<record> results;        
        for (auto& db_path : list_dir(_path, false))
        {
            try
            {
                sql::sqlite db(db_path);
                results.splice(results.end(), db.search(statement));
            }
            catch (std::exception& e)
            {
                // query error -> corrupted
                auto bak_name = db_path + ".corrupt";
                if (std::rename(db_path.c_str(), bak_name.c_str()) != 0)
                    ; // log error here
            }
        }
        return results;
    }

    void database::update(const std::list<std::string>& statements)
    {
        lock_db();
        defer{ unlock_db(); };

        _db->begin_transaction();
        for (auto& statement : statements)
            _db->exec(statement);
        _db->commit_transaction();
    }
}