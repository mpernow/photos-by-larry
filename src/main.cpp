#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Photos by Larry");
    QApplication::setOrganizationName("Larry");

    MainWindow window;
    window.show();

    return app.exec();
}
