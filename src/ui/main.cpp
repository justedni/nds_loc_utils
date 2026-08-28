#include "QtNDSLocUtils.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtNDSLocUtils w;
    w.show();
    return a.exec();
}
