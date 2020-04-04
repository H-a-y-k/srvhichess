#include <QCoreApplication>
#include "hichess_server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Hichess::Server srv;

    return a.exec();
}
