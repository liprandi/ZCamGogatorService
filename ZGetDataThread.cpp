#include "ZGetDataThread.h"
#include <chrono>
#include <QMessageBox>
#include <QDateTime>

using namespace std::chrono_literals;


#define RECEIVE_TIMEOUT         10000000
#define INVALID_RANGE_16BIT     ((k16s)0x8000)          // gocator transmits range data as 16-bit signed integers. 0x8000 signifies invalid range data. 
#define DOUBLE_MAX              ((k64f)1.7976931348623157e+308) // 64-bit double - largest positive value.  
#define INVALID_RANGE_DOUBLE    ((k64f)-DOUBLE_MAX)             // floating point value to represent invalid range data.
//#define SENSOR_IP               "121.31.78.86"  

#define NM_TO_MM(VALUE) (((k64f)(VALUE))/1000000.0)
#define UM_TO_MM(VALUE) (((k64f)(VALUE))/1000.0)

typedef struct ProfilePoint
{
    double x;   // x-coordinate in engineering units (mm) - position along laser line
    double y;   // y-coordinate in engineering units (mm) - position along the direction of travel
    double z;   // z-coordinate in engineering units (mm) - height (at the given x position)
    unsigned long index;
} ProfilePoint;

ZGetDataThread::ZGetDataThread(ZQueue<ZMeasurement>& queue, QObject* parent) :
    QThread(parent)
    , m_quit(false)
    , m_run(false)
    , m_data(queue)
{
    api = kNULL;
    system = kNULL;
    sensor = kNULL;
    dataset = kNULL;
    measurementData = kNULL;
    measurement = kNULL;
    collection_tools = kNULL;
}

ZGetDataThread::~ZGetDataThread()
{
    if (m_run)
    {
        m_quit = true;
        while(m_run)
            sleep(1);
 
        GoDestroy(system);
        GoDestroy(api);
    }
}

void ZGetDataThread::init(const QString& addr, const QString& imagesDir)
{
    m_dir = imagesDir;
    // construct Gocator API Library
    if ((status = GoSdk_Construct(&api)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSdk_Construct:%1?")).arg(status),
            QMessageBox::Discard);
        return;
    }

    // construct GoSystem object
    if ((status = GoSystem_Construct(&system, kNULL)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSystem_Construct:%1?")).arg(status),
            QMessageBox::Discard);
        return;
    }

    // Parse IP address into address data structure
    kIpAddress_Parse(&ipAddress, addr.toLatin1().data());

    // obtain GoSensor object by sensor IP address
    if ((status = GoSystem_FindSensorByIpAddress(system, &ipAddress, &sensor)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSystem_FindSensorByIpAddress:%1?")).arg(status),
            QMessageBox::Discard);
        return;
    }

    // create connection to GoSensor object
    if ((status = GoSensor_Connect(sensor)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSensor_Connect:%1?")).arg(status),
            QMessageBox::Discard);
        return;
    }

    // enable sensor data channel
    if ((status = GoSystem_EnableData(system, kTRUE)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSystem_EnableData:%1?")).arg(status),
            QMessageBox::Discard);
        return;
    }
    start();
}
void ZGetDataThread::run()
{
    QDateTime dt = QDateTime::currentDateTime();
    QString dtstr = dt.toString("yyyy-MM-dd hh:mm:ss");
    QString pictstr = m_job + dt.toString("_yyyyMMddhhmmss.obj");
    m_run = true;
    int step = 0;
    readCollectionTools(sensor, &collection_tools);

    while (!m_quit)
    {
        if (sensor == kNULL || !GoSensor_IsConnected(sensor))
        {
            if (!reconnect())
            {
                sleep(1s);
                continue;
            }
        }
        while (GoSystem_ReceiveData(system, &dataset, RECEIVE_TIMEOUT) == kOK)
        {
            // each result can have multiple data items
            // loop through all items in result message
            bool data_valid = true;
            for (i = 0; i < GoDataSet_Count(dataset); ++i)
            {
                GoDataMsg dataObj = GoDataSet_At(dataset, i);
                GoDataMessageType type = GoDataMsg_Type(dataObj);
                switch (type)
                {
                    case GO_DATA_MESSAGE_TYPE_STAMP:
                    {
                        timestamp(dataObj);
                    }
                    break;
                    case GO_DATA_MESSAGE_TYPE_UNIFORM_SURFACE:
                    {
                        qDebug() << "----GO_DATA_MESSAGE_TYPE_UNIFORM_SURFACE: " << i;
                        dt = QDateTime::currentDateTime();
                        dtstr = dt.toString("yyyy-MM-dd hh:mm:ss");
                        char filename[256];
 
                        kBool changed;
                        if (GoSensor_LoadedJob(sensor, filename, 256, &changed) == kOK)
                        {
                            m_job = QString(filename).replace(".job", "", Qt::CaseInsensitive);
                            data_valid = (m_job.compare("INS3L") == 0 || m_job.compare("INS12L") == 0);
                            m_unsaved = changed;
                        }
                        if(data_valid)
                        {
                            pictstr = m_dir + m_job + dt.toString("_yyyyMMddhhmmss.obj");
                            readCollectionTools(sensor, &collection_tools);
                            getImage(dataObj, pictstr);
                            ZMeasurement data;
                            data.dt = dtstr;
                            data.id = m_unsaved ? 9998 : 9999;
                            data.value = 0;
                            data.decision = GO_DECISION_PASS;
                            data.decisionCode = GO_DECISION_CODE_OK;
                            data.description = pictstr;
                            m_data.enqueue(data);
                        }
                    }
                    break;
                    case GO_DATA_MESSAGE_TYPE_MEASUREMENT:
                    {
                        if (data_valid)
                        {
                            measure(dataObj, dtstr);
                        }
                    }
                    break;
                /*     case GO_DATA_MESSAGE_TYPE_FEATURE_POINT:
                    {
                        qDebug() << "----GO_DATA_MESSAGE_TYPE_FEATURE_POINT: " << i;
                        GoPointFeatureMsg featurePointMsg = dataObj;


                    }*/
                    default:
                    {
                        qDebug() << "----" << type << ": " << i;
                    }
                    break;
                }
            }
            //free memory arrays
            GoDestroy(dataset);
            dataset = kNULL;
        }
        sleep(10ms);
    }
    m_run = false;
    m_quit = false;
}
bool ZGetDataThread::reconnect()
{
    // obtain GoSensor object by sensor IP address
    if ((status = GoSystem_FindSensorByIpAddress(system, &ipAddress, &sensor)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSystem_FindSensorByIpAddress"),
            QString(tr("Error: GoSystem_FindSensorByIpAddress:%1?")).arg(status),
            QMessageBox::Discard);
        sensor = kNULL;
        return false;
    }

    // create connection to GoSensor object
    if ((status = GoSensor_Connect(sensor)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSensor_Connect:%1?")).arg(status),
            QMessageBox::Discard);
        sensor = kNULL;
        return false;
    }

    // enable sensor data channel
    if ((status = GoSystem_EnableData(system, kTRUE)) != kOK)
    {
        QMessageBox::information(NULL, tr("GoSdk connection error"),
            QString(tr("Error: GoSystem_EnableData:%1?")).arg(status),
            QMessageBox::Discard);
        return false;
    }
    return true;
}

/*
* Receive timestamp information data
*/
void ZGetDataThread::timestamp(GoDataMsg dataObj)
{
    qDebug() << "----GO_DATA_MESSAGE_TYPE_STAMP: " << i;
    GoStampMsg stampMsg = dataObj;
    qDebug() << "\tStamp Message batch count: " << (k32u)GoStampMsg_Count(stampMsg);

    for (j = 0; j < GoStampMsg_Count(stampMsg); j++)
    {
        GoStamp* stamp = GoStampMsg_At(stampMsg, j);
        qDebug() << "\tTimestamp: " << stamp->timestamp;
        qDebug() << "\tEncoder position at leading edge: " << stamp->encoder;
        qDebug() << "\tFrame index: " << stamp->frameIndex;
    }
}

/*
* Receive measure information data
*/
void ZGetDataThread::measure(GoDataMsg dataObj, const QString& dtstr)
{
    qDebug() << "----GO_DATA_MESSAGE_TYPE_MEASUREMENT: " << i;
    GoMeasurementMsg measurementMsg = dataObj;

    qDebug() << "Measurement Message batch count: ", (k32u)GoMeasurementMsg_Count(measurementMsg);

    for (k = 0; k < GoMeasurementMsg_Count(measurementMsg); ++k)
    {
        ZMeasurement data;
        measurementData = GoMeasurementMsg_At(measurementMsg, k);
        // 1. Retrieve the measurement ID
        int l = GoMeasurementMsg_Id(measurementMsg);
        data.dt = dtstr;
        data.id = l;
        data.value = measurementData->value;
        data.decision = measurementData->decision;
        data.decisionCode = measurementData->decisionCode;
        //2. Retrieve the measurement from the set of tools using measurement ID

        if (collection_tools != kNULL && (measurement = GoTools_FindMeasurementById(collection_tools, l)) != kNULL)
        {
            data.description = GoMeasurement_Name(measurement);
        }
        else
        {
            data.description = "---";
        }
        m_data.enqueue(data);
    }
}

/*
* Receive cloud points data e tranform it in obj 3d file
*/
void ZGetDataThread::getImage(GoDataMsg dataObj, const QString& picture)
{
    GoSurfaceMsg surfaceMsg = dataObj;
    ProfilePoint** surfaceBuffer = NULL;
    k32u surfaceBufferHeight = 0;
    unsigned int j, k;

    unsigned int rowIdx, colIdx;
    unsigned int numofpoints = 0;
    double XResolution = NM_TO_MM(GoSurfaceMsg_XResolution(surfaceMsg));
    double YResolution = NM_TO_MM(GoSurfaceMsg_YResolution(surfaceMsg));
    double ZResolution = NM_TO_MM(GoSurfaceMsg_ZResolution(surfaceMsg));
    double XOffset = UM_TO_MM(GoSurfaceMsg_XOffset(surfaceMsg));
    double YOffset = UM_TO_MM(GoSurfaceMsg_YOffset(surfaceMsg));
    double ZOffset = UM_TO_MM(GoSurfaceMsg_ZOffset(surfaceMsg));

    unsigned int width = (unsigned int)GoSurfaceMsg_Width(surfaceMsg);
    unsigned int length = (unsigned int)GoSurfaceMsg_Length(surfaceMsg);

    // printf("  Surface data width: %lu\n", (k32u)width);
    // printf("  Surface data length: %lu\n", (k32u)length);

    FILE* fobj = fopen(picture.toLatin1(), "w");

    if (fobj != NULL)
    {
        fprintf(fobj, "#\n# Surface from Gocator (%u,%u)\n#", width, length);
        fprintf(fobj, "# Surface data width: %lu\n", (k32u)width);
        fprintf(fobj, "# Surface data length: %lu\n", (k32u)length);
        fprintf(fobj, "# Surface data XResolution: %f\n", XResolution);
        fprintf(fobj, "# Surface data YResolution: %f\n", YResolution);
        fprintf(fobj, "# Surface data ZResolution: %f\n", ZResolution);
        fprintf(fobj, "# Surface data XOffset: %f\n", XOffset);
        fprintf(fobj, "# Surface data YOffset: %f\n", YOffset);
        fprintf(fobj, "# Surface data ZOffset: %f\n", ZOffset);
    }
    //allocate memory if needed
    if (surfaceBuffer == NULL)
    {

        surfaceBuffer = (ProfilePoint **)(malloc(length * sizeof(ProfilePoint*)));
        if (surfaceBuffer)
        {
            for (j = 0; j < length; j++)
            {
                surfaceBuffer[j] = (ProfilePoint*)(malloc(width * sizeof(ProfilePoint)));
                if (!surfaceBuffer[j])
                    exit(-201);
            }
        }
        else
            exit(-200);

        surfaceBufferHeight = (k32u)length;
    }

    for (rowIdx = 0; rowIdx < length; rowIdx++)
    {
        k16s* data = GoSurfaceMsg_RowAt(surfaceMsg, rowIdx);

        for (colIdx = 0; colIdx < width; colIdx++)
        {
            // gocator transmits range data as 16-bit signed integers
            // to translate 16-bit range data to engineering units, the calculation for each point is: 
            //          X: XOffset + columnIndex * XResolution 
            //          Y: YOffset + rowIndex * YResolution
            //          Z: ZOffset + height_map[rowIndex][columnIndex] * ZResolution

            double x = XOffset + XResolution * colIdx;
            double y = YOffset + YResolution * rowIdx;


            surfaceBuffer[rowIdx][colIdx].x = x;
            surfaceBuffer[rowIdx][colIdx].y = y;


            if (data[colIdx] != INVALID_RANGE_16BIT)
            {
                double z = ZOffset + ZResolution * data[colIdx];
                surfaceBuffer[rowIdx][colIdx].z = z;
                surfaceBuffer[rowIdx][colIdx].index = numofpoints + 1;

                if (fobj != NULL)
                {
                    fprintf(fobj, "\nv %.3f %.3f %.3f", x, y, z);
                    numofpoints++;
                }
            }
            else
            {
                surfaceBuffer[rowIdx][colIdx].z = INVALID_RANGE_DOUBLE;
                surfaceBuffer[rowIdx][colIdx].index = 0;
/*              if (fobj != NULL)
                {
                    fprintf(fobj, "\nv %.3f %.3f -100.000", x, y);
                    numofpoints++;
                }*/
            }
        }
    }
    if (fobj != NULL)
    {
        fprintf(fobj, "\n\n\nvn 0.000 0.000 1.000\n\n\n#");
        for (rowIdx = 0; rowIdx < (length - 1); rowIdx++)
        {
            for (colIdx = 0; colIdx < (width - 1); colIdx++)
            {
                int idxbase00 = surfaceBuffer[rowIdx][colIdx].index;
                int idxbase01 = surfaceBuffer[rowIdx][colIdx + 1].index;
                int idxbase10 = surfaceBuffer[rowIdx + 1][colIdx].index;
                int idxbase11 = surfaceBuffer[rowIdx + 1][colIdx + 1].index;
                bool ok1 = (idxbase00 != 0) && (idxbase01 != 0) && (idxbase10 != 0);
                bool ok2 = (idxbase01 != 0) && (idxbase10 != 0) && (idxbase11 != 0);

                if (ok1)
                {
                    fprintf(fobj, "\nf %u %u %u", idxbase00, idxbase01, idxbase10);
                }
                if (ok2)
                {
                    fprintf(fobj, "\nf %u %u %u", idxbase01, idxbase11, idxbase10);
                }
            }
        }
        fflush(fobj);
        fclose(fobj);
    }
    if (surfaceBuffer)
    {
        for (k = 0; k < surfaceBufferHeight; k++)
        {
            free(surfaceBuffer[k]);
        }
    }
}

void ZGetDataThread::readCollectionTools(GoSensor sensor, GoTools* pcollection_tools)
{
    kSize prev_count = 0;
    if (*pcollection_tools != kNULL)
        prev_count = GoTools_ToolCount(*pcollection_tools);
    if ((*pcollection_tools = GoSensor_Tools(sensor)) == kNULL)
    {
        printf("Error: GoSensor_Tools: Invalid Handle\n");
        return;
    }
    else
    {
        if (GoTools_ToolCount(*pcollection_tools) != prev_count)
            examineCollectionTools(*pcollection_tools);
    }
}
void ZGetDataThread::examineCollectionTools(GoTools collection_tools)
{
    unsigned int i, k;
    char nametool[256];
    GoMeasurement meas = kNULL;

    kSize count = GoTools_ToolCount(collection_tools);
    for (i = 0; i < count; i++)
    {
        GoTool gtool = kNULL;
        gtool = GoTools_ToolAt(collection_tools, i);
        if (gtool != kNULL)
        {
            printf("Tool %d\n", i);
            printf("\tid     = %d\n", GoTool_Id(gtool));
            if (GoTool_Name(gtool, nametool, 256) == kOK)
            {
                printf("\tname   = %s\n", nametool);
                for (k = 0; k < GoTool_MeasurementCount(gtool); k++)
                {
                    meas = GoTool_MeasurementAt(gtool, k);
                    if (meas != kNULL)
                    {
                        printf("\t\tMeasurement %d\n", i);
                        printf("\t\t\tid     = %d\n", GoMeasurement_Id(meas));
                        printf("\t\t\tname   = %s\n", GoMeasurement_Name(meas));
                        printf("\t\t\tenabled= %d\n\n", GoMeasurement_Enabled(meas));
                    }
                }
            }
        }
    }
}