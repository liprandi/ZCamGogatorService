#include "ZCamGogatorService.h"
#include <chrono>
#include <QLabel>

using namespace std::chrono_literals;


ZCamGogatorService::ZCamGogatorService(QWidget *parent)
    : QMainWindow(parent)
    , m_rr(&m_data_rr)
    , m_rl(&m_data_rl)
    , m_fr(&m_data_fr)
    , m_fl(&m_data_fl)
{
    ui.setupUi(this);
    m_fr.init("121.31.78.86");      // op50
    startTimer(200ms);
}

ZCamGogatorService::~ZCamGogatorService()
{
}
void ZCamGogatorService::timerEvent(QTimerEvent* /*event*/)
{
    ZMeasurement data;
    if (!m_data_rr.empty())
    {
        data = m_data_rr.dequeue();
        ui.cntRead->setText(QString("%1").arg(data.value));
    }
    if (!m_data_rl.empty())
    {
        data = m_data_rl.dequeue();
        ui.cntRead->setText(QString("%1").arg(data.value));
    }
    if (!m_data_fr.empty())
    {
        data = m_data_fr.dequeue();
        ui.cntRead->setText(QString("%1").arg(data.value));
    }
    if (!m_data_fl.empty())
    {
        data = m_data_fl.dequeue();
        ui.cntRead->setText(QString("%1").arg(data.value));
    }
}
