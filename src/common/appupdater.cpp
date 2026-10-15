#include "appupdater.h"

#include <QApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>

AppUpdater::AppUpdater(QWidget* window, QNetworkAccessManager* manager):
    QObject(window), window(window), network(manager ? manager : new QNetworkAccessManager(this))
{
}

void AppUpdater::finishProgress()
{
    if(progress)
    {
        progress->close();
        progress->deleteLater();
        progress = nullptr;
    }
    busy = false;
}

void AppUpdater::fail(const QString& message)
{
    finishProgress();
    QMessageBox::warning(window, tr("App Updates"), message);
}

void AppUpdater::check()
{
    if(busy)
        return;
    busy = true;
    progress = new QProgressDialog(tr("Checking GitHub for updates..."), tr("Cancel"), 0, 0, window);
    progress->setWindowTitle(tr("App Updates"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    QNetworkRequest request(QUrl("https://api.github.com/repos/unicastbg/Proxmark3GUI-Modern/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Proxmark3GUI-Modern/" + QApplication::applicationVersion().toLatin1());
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = network->get(request);
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if(reply->bytesAvailable() > 1024 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const bool cancelled = reply->error() == QNetworkReply::OperationCanceledError;
        const QString error = reply->errorString();
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        const QByteArray data = reply->readAll();
        finishProgress();
        if(cancelled)
            return;
        if(!ok)
        {
            fail(tr("Could not check for updates. Check your connection and try again.\n\n%1").arg(error));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(data, &parseError);
        if(parseError.error != QJsonParseError::NoError || !json.isObject())
        {
            fail(tr("GitHub returned an invalid release response."));
            return;
        }
        release = AppUpdateInfo::parse(json.object());
        if(release.version.isNull())
        {
            fail(tr("The latest release version could not be read."));
            return;
        }
        showRelease();
    });
    progress->show();
}

void AppUpdater::showRelease()
{
    const QVersionNumber current = QVersionNumber::fromString(QApplication::applicationVersion());
    if(current.isNull())
    {
        fail(tr("The installed app version could not be read."));
        return;
    }
    if(QVersionNumber::compare(release.version, current) <= 0)
    {
        QMessageBox::information(window, tr("App Updates"),
            tr("You are up to date.\n\nInstalled: %1\nLatest release: %2")
                .arg(current.toString(), release.version.toString()));
        return;
    }

    QMessageBox box(QMessageBox::Information, tr("Update Available"),
        tr("Proxmark3GUI Modern %1 is available.\nInstalled: %2")
            .arg(release.version.toString(), current.toString()), QMessageBox::Cancel, window);
    box.setTextFormat(Qt::PlainText);
    box.setDetailedText(release.notes.isEmpty() ? tr("No release notes were provided.") : release.notes);
    QPushButton* downloadButton = nullptr;
    if(!release.downloadUrl.isEmpty())
        downloadButton = box.addButton(tr("Download Update"), QMessageBox::AcceptRole);
    else
        box.setInformativeText(tr("A verified Windows installer is not available for this release."));
    QPushButton* releasesButton = box.addButton(tr("View Release"), QMessageBox::ActionRole);
    box.exec();
    if(downloadButton && box.clickedButton() == downloadButton)
        download();
    else if(box.clickedButton() == releasesButton)
        QDesktopServices::openUrl(QUrl("https://github.com/unicastbg/Proxmark3GUI-Modern/releases/latest"));
}

void AppUpdater::download()
{
    downloadDir.reset(new QTemporaryDir());
    if(!downloadDir->isValid())
    {
        fail(tr("Could not create a temporary folder for the update."));
        return;
    }
    downloadFile.reset(new QFile(downloadDir->filePath("Proxmark3GUI-Modern-setup.exe")));
    if(!downloadFile->open(QIODevice::WriteOnly))
    {
        fail(tr("Could not save the update installer."));
        return;
    }
    busy = true;
    downloadError.clear();
    progress = new QProgressDialog(tr("Downloading update..."), tr("Cancel"), 0, 100, window);
    progress->setWindowTitle(tr("App Updates"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    QNetworkRequest request(release.downloadUrl);
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    QNetworkReply* reply = network->get(request);
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl& url) {
        if(AppUpdateInfo::allowedDownloadUrl(url))
            reply->redirectAllowed();
        else
        {
            downloadError = tr("The installer download redirected to an unexpected location.");
            reply->abort();
        }
    });
    auto saveData = [this, reply]() {
        if(!downloadError.isEmpty())
            return;
        const QByteArray chunk = reply->readAll();
        if(downloadFile->size() + chunk.size() > release.size)
            downloadError = tr("The installer download exceeded its expected size.");
        else if(downloadFile->write(chunk) != chunk.size())
            downloadError = tr("Could not save the update installer. Check available disk space.");
        if(!downloadError.isEmpty())
            reply->abort();
    };
    connect(reply, &QNetworkReply::readyRead, this, saveData);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
        if(progress)
            progress->setValue(static_cast<int>(qMin<qint64>(100, received * 100 / release.size)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, saveData]() {
        reply->deleteLater();
        if(reply->error() == QNetworkReply::NoError)
            saveData();
        const bool cancelled = reply->error() == QNetworkReply::OperationCanceledError;
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        const QString error = reply->errorString();
        downloadFile->close();
        finishProgress();
        if(!downloadError.isEmpty())
        {
            downloadFile->remove();
            fail(downloadError);
            return;
        }
        if(cancelled)
        {
            downloadFile->remove();
            return;
        }
        if(!ok || !AppUpdateInfo::verifyInstaller(downloadFile->fileName(), release))
        {
            downloadFile->remove();
            fail(ok ? tr("Installer verification failed. The download was discarded.")
                    : tr("Could not download the update.\n\n%1").arg(error));
            return;
        }
        emit installRequested();
    });
    progress->show();
}

void AppUpdater::launchInstaller()
{
    if(!downloadFile || !AppUpdateInfo::verifyInstaller(downloadFile->fileName(), release))
    {
        fail(tr("The verified installer is no longer available. Check for updates again."));
        return;
    }
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(downloadFile->fileName())))
    {
        fail(tr("Windows could not start the installer. You can download it from the GitHub release page."));
        return;
    }
    // Setup needs the installer to survive this app's exit.
    downloadDir->setAutoRemove(false);
    QApplication::quit();
}
