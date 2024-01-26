#pragma once
#include <mariadb/conncpp.hpp>

using namespace sql;

class ZDatabase
{

public:
    explicit ZDatabase();
    ~ZDatabase();

    bool connect(const std::string& host = "localhost", const std::string& user = "root", const std::string& password = "admin", const std::string& database = "MariaDB", int port = 3306);

public:
    bool query(const std::string& qString);
    inline bool isconnected()
    {
        return m_driver && m_conn && (m_conn->isValid() || m_conn->reconnect());
    }

private:    // data used by MariaDb library
    Driver* m_driver;
    SQLString m_url;
    Properties m_properties;
    Connection* m_conn;
    ResultSet* m_res;
};

