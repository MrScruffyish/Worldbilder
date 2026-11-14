#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Worldbilder"));
    QApplication::setOrganizationName(QStringLiteral("Worldbilder"));

    MainWindow window;
    window.show();

    return app.exec();
}
