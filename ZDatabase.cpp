#include "ZDatabase.h"
#include <QString>
#include <QDebug>
#include <memory>
#include <format>

ZDatabase::ZDatabase() 
{
    // Instantiate Driver
    m_conn = nullptr;
    m_res = nullptr;
    m_driver = mariadb::get_driver_instance();
    if (m_driver)
    {
        if (connect())
        {
            query("show databases");
            query("use gocator");
            query("SHOW TABLES");
        }
    }
}

ZDatabase::~ZDatabase()
{
    m_conn->close();
    delete m_conn;
    m_conn = nullptr;
}

bool ZDatabase::connect(const std::string& host, const std::string& user, const std::string& password, const std::string& database, int port)
{
    // Configure Connection
/*    std::string url = std::format("jdbc:mariadb://{}:{}/{}", host, port, database);
    m_url = SQLString(url.data());
    m_properties = Properties({
                                {"user", user.data()}
                              , {"password", password.data()}
                              }); 
                              */
    // Establish Connection
    try
    {
//      m_conn = m_driver->connect(m_url, m_properties);
        m_conn = m_driver->connect(host.data(), user.data(), password.data());
    }
    catch(sql::SQLException& e)
    {
        qDebug() << "Error selecting: " << e.what();
    }
    return m_conn != nullptr;
}
bool ZDatabase::query(const std::string& qString)
{
    Statement* stmnt = nullptr;
    try 
    {
        // Create a new Statement
        std::unique_ptr<Statement> stmnt(m_conn->createStatement());
        if (m_res)
        {
            m_res->close();
            delete m_res;
            m_res = nullptr;
        }
        // Execute query
        m_res = stmnt->executeQuery(qString.data());
    }
    catch (sql::SQLException& e) 
    {
        qDebug() << "Error selecting: " << e.what();
    }
    return true;
}
