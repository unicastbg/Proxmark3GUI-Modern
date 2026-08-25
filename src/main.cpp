#include "ui/mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QSettings>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif
#include <QDir>

int main(int argc, char *argv[])
{
    // A trick to handle non-ascii path
    // The application cannot find the plugins when the path contains non ascii characters.
    // However, the plugins will be load after creating MainWindow(or QApplication?).
    // QDir will handle the path correctly.
    QDir* pluginDir = new QDir;
    if(pluginDir->cd("plugins")) // has plugins folder
    {
        qputenv("QT_PLUGIN_PATH", pluginDir->absolutePath().toLocal8Bit());
    }
    delete pluginDir;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/modern/app_icon.ico"));

    QSettings settings("GUIsettings.ini", QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    settings.beginGroup("UI");
    QString theme = settings.value("Theme_Name", "modern_dark").toString();
    settings.endGroup();

    QFile* themeFile = new QFile();
    QTextStream* themeStream = new QTextStream();
    QString qssString = a.styleSheet(); // default behavior
    if(theme == "(none)")
        ;
    else if(theme == "modern_dark")
    {
        themeFile->setFileName(":/modern/modern_dark.qss");
        themeFile->open(QFile::ReadOnly | QFile::Text);
        themeStream->setDevice(themeFile);
        qssString = themeStream->readAll();
    }
    else if(theme == "qdss_dark")
    {
        themeFile->setFileName(":/qdarkstyle/dark/darkstyle.qss");
        themeFile->open(QFile::ReadOnly | QFile::Text);
        themeStream->setDevice(themeFile);
        qssString = themeStream->readAll();
    }
    else if(theme == "qdss_light")
    {
        themeFile->setFileName(":/qdarkstyle/light/lightstyle.qss");
        themeFile->open(QFile::ReadOnly | QFile::Text);
        themeStream->setDevice(themeFile);
        qssString = themeStream->readAll();
    }
    a.setStyleSheet(qssString);
    delete themeFile;
    delete themeStream;
    themeFile = nullptr;
    themeStream = nullptr;

    MainWindow w;
    w.initUI();
    w.show();
    return a.exec();
}


