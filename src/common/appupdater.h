#ifndef APPUPDATER_H
#define APPUPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProgressDialog>
#include <QTemporaryDir>
#include <QFile>
#include <memory>
#include "appupdateinfo.h"

class AppUpdater : public QObject
{
    Q_OBJECT
public:
    explicit AppUpdater(QWidget* window, QNetworkAccessManager* manager = nullptr);
    void check();
    void launchInstaller();

signals:
    void installRequested();

private:
    QWidget* window;
    QNetworkAccessManager* network;
    QPointer<QProgressDialog> progress;
    AppUpdateInfo::Release release;
    std::unique_ptr<QTemporaryDir> downloadDir;
    std::unique_ptr<QFile> downloadFile;
    bool busy = false;
    QString downloadError;

    void showRelease();
    void download();
    void finishProgress();
    void fail(const QString& message);
};

#endif
