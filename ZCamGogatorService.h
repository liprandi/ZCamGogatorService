#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ZCamGogatorService.h"
#include  "ZGetDataThread.h"
#include  "ZQueue.h"
#include  "ZDatabase.h"



class ZCamGogatorService : public QMainWindow
{
    Q_OBJECT

public:
    ZCamGogatorService(QWidget *parent = nullptr);
    ~ZCamGogatorService();

protected:
    void timerEvent(QTimerEvent* event) override;
private:
    void writeRecord(const char* table, const ZMeasurement& data);
private:
    Ui::ZCamGogatorServiceClass ui;
    ZQueue<ZMeasurement> m_data_rr;
    ZQueue<ZMeasurement> m_data_fr;
    ZQueue<ZMeasurement> m_data_rl;
    ZQueue<ZMeasurement> m_data_fl;
    ZGetDataThread m_rr;    // op040 rear right door
    ZGetDataThread m_fr;    // op050 front right door
    ZGetDataThread m_rl;    // op090 rear left door
    ZGetDataThread m_fl;    // op0100 front left door
    ZDatabase m_database;    // MariaDB connection
};
