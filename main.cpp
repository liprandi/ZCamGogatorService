#include "ZCamGogatorService.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ZCamGogatorService w;
    w.show();
    return a.exec();
}
