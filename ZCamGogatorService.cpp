#include "ZCamGogatorService.h"
#include <chrono>
#include <QLabel>
#include <format>

using namespace std::chrono_literals;


ZCamGogatorService::ZCamGogatorService(QWidget *parent)
    : QMainWindow(parent)
    , m_rr(&m_data_rr)
    , m_rl(&m_data_rl)
    , m_fr(&m_data_fr)
    , m_fl(&m_data_fl)
{
    ui.setupUi(this);
    m_rr.init("121.31.78.82", "./images/rr/");      // op40
    m_fr.init("121.31.78.86", "./images/fr/");      // op50
    m_rl.init("121.31.78.90", "./images/rl/");      // op90
    m_fl.init("121.31.78.94", "./images/fl/");      // op100
    startTimer(200ms);
}

ZCamGogatorService::~ZCamGogatorService()
{
    m_rr.switchoff();
    m_rl.switchoff();
    m_fr.switchoff();
    m_fl.switchoff();
    m_rr.wait();
    m_rl.wait();
    m_fr.wait();
    m_fl.wait();
}

void ZCamGogatorService::writeRecord(const char* table, const ZMeasurement& data)
{
    if((data.decision == GO_DECISION_PASS || data.decisionCode == GO_DECISION_CODE_OK) && m_database.isconnected())
    {
        std::string qstr = std::format("INSERT INTO {} (datetime, id, value, description, decision, decisionCode) "
                                       "VALUES('{}', {}, {}, '{}', {}, {})", 
                                       table,
                                       data.dt.toStdString(), data.id, data.value, data.description.toStdString(),
                                       data.decision, data.decisionCode);
        m_database.query(qstr);
    }
}

void ZCamGogatorService::timerEvent(QTimerEvent* /*event*/)
{
    ZMeasurement data;
    while(!m_data_rr.empty())
    {
        data = m_data_rr.dequeue();
        writeRecord("rr_data", data);
        if (data.decision == GO_DECISION_PASS || data.decisionCode == GO_DECISION_CODE_OK)
        {
             ui.cntRead->setText(QString("%1").arg(data.value));
        }
    }
    if (!m_data_rl.empty())
    {
        data = m_data_rl.dequeue();
        writeRecord("rl_data", data);
        if (data.decision == GO_DECISION_PASS || data.decisionCode == GO_DECISION_CODE_OK)
        {
            ui.cntRead->setText(QString("%1").arg(data.value));
        }
    }
    if (!m_data_fr.empty())
    {
        data = m_data_fr.dequeue();
        writeRecord("fr_data", data);
        if (data.decision == GO_DECISION_PASS || data.decisionCode == GO_DECISION_CODE_OK)
        {
            ui.cntRead->setText(QString("%1").arg(data.value));
        }
    }
    if (!m_data_fl.empty())
    {
        data = m_data_fl.dequeue();
        writeRecord("fl_data", data);
        if (data.decision == GO_DECISION_PASS || data.decisionCode == GO_DECISION_CODE_OK)
        {
            ui.cntRead->setText(QString("%1").arg(data.value));
        }
    }
}

