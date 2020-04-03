#include <QCoreApplication>
#include "hichess_server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Server::HichessServer srv;

    return a.exec();
}
