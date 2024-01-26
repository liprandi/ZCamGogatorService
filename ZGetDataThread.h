#pragma once
#include <QThread>
#include <GoSdk/GoSdk.h>
#include "ZQueue.h"

struct ZMeasurement
{
    QString dt;
    int     id = 0;                 // id = 9999 means picture file name
    double  value = 0.;
    bool    decision = GO_DECISION_PASS;
    bool    decisionCode = GO_DECISION_CODE_OK;
    QString description;        // in case of picture here is file name
};

class ZGetDataThread :public QThread
{
    Q_OBJECT
public:
    explicit ZGetDataThread(ZQueue<ZMeasurement>* queue, QObject* parent = nullptr);
    ~ZGetDataThread() override;
    void run() override;
private:
    void getImage(GoDataMsg dataObj, const QString& picture);
    void readCollectionTools(GoSensor sensor, GoTools* collection_tools);
    void examineCollectionTools(GoTools collection_tools);

public:
    void init(const QString& addr);

private:
     bool m_quit;
     bool m_run;
     // data of camera
     kAssembly api;
     kStatus status;
     GoSystem system;
     GoSensor sensor;
     GoDataSet dataset;
     unsigned int i, j, k;
     kIpAddress ipAddress;
     GoMeasurementData* measurementData;
     GoMeasurement measurement;
     GoTools collection_tools;
     ZQueue<ZMeasurement>* m_data;
     QString m_job;
     bool    m_unsaved;
};

