#include <QtTest>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QNetworkReply>
#include <QTimer>
#include <QAbstractButton>
#include <QPushButton>
#include <QJsonDocument>
#include "../../src/common/appupdater.h"
#include "../../src/common/appupdateinfo.h"

class TestReply : public QNetworkReply
{
public:
    TestReply(const QNetworkRequest& request, const QByteArray& data, int status, int delay, QObject* parent):
        QNetworkReply(parent), data(data)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if(status != 200)
            setError(QNetworkReply::ContentNotFoundError, "Test HTTP error");
        open(QIODevice::ReadOnly);
        QTimer::singleShot(delay, this, [this]() {
            if(isFinished())
                return;
            emit readyRead();
            if(isFinished())
                return;
            setFinished(true);
            emit finished();
        });
    }
    void abort() override
    {
        if(isFinished())
            return;
        setError(QNetworkReply::OperationCanceledError, "Cancelled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return data.size() + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char* buffer, qint64 maxSize) override
    {
        const qint64 size = qMin<qint64>(maxSize, data.size());
        if(size == 0)
            return -1;
        memcpy(buffer, data.constData(), size);
        data.remove(0, size);
        return size;
    }
private:
    QByteArray data;
};

class TestNetwork : public QNetworkAccessManager
{
public:
    QByteArray metadata;
    QByteArray installer = "abc";
    int metadataStatus = 200;
    int requests = 0;
    int delay = 0;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        ++requests;
        return new TestReply(request, requests == 1 ? metadata : installer,
                             requests == 1 ? metadataStatus : 200, delay, this);
    }
};

class AppUpdateInfoTest : public QObject
{
    Q_OBJECT

    QJsonObject fixture() const
    {
        const QJsonObject asset {
            {"name", "Proxmark3GUI-Modern-0.4.0-setup.exe"},
            {"size", 3},
            {"digest", "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
            {"browser_download_url", "https://github.com/unicastbg/Proxmark3GUI-Modern/releases/download/v0.4.0/Proxmark3GUI-Modern-0.4.0-setup.exe"}
        };
        return {{"tag_name", "v0.4.0"}, {"body", "Release notes"}, {"assets", QJsonArray {asset}}};
    }

private slots:
    void initTestCase()
    {
        QApplication::setApplicationVersion("0.3.3");
    }

    void updateWorkflow_data()
    {
        QTest::addColumn<QByteArray>("installer");
        QTest::addColumn<bool>("installExpected");
        QTest::newRow("verified") << QByteArray("abc") << true;
        QTest::newRow("corrupted") << QByteArray("abd") << false;
        QTest::newRow("truncated") << QByteArray("ab") << false;
        QTest::newRow("oversized") << QByteArray("abcd") << false;
    }

    void updateWorkflow()
    {
        QFETCH(QByteArray, installer);
        QFETCH(bool, installExpected);
        QWidget window;
        TestNetwork network;
        network.metadata = QJsonDocument(fixture()).toJson();
        network.installer = installer;
        AppUpdater updater(&window, &network);
        QSignalSpy install(&updater, &AppUpdater::installRequested);
        bool sawError = false;
        QTimer responder;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(!box)
                return;
            for(QAbstractButton* button : box->buttons())
            {
                if(button->text() == "Download Update")
                {
                    button->click();
                    return;
                }
            }
            sawError = true;
            box->accept();
        });
        responder.start(10);
        updater.check();
        if(installExpected)
            QTRY_COMPARE(install.count(), 1);
        else
        {
            QTRY_VERIFY(sawError);
            QCOMPARE(install.count(), 0);
        }
        QCOMPARE(network.requests, 2);
    }

    void upToDateAndNetworkError_data()
    {
        QTest::addColumn<int>("status");
        QTest::newRow("current") << 200;
        QTest::newRow("network-error") << 404;
    }

    void cancellation_data()
    {
        QTest::addColumn<int>("cancelRequest");
        QTest::newRow("check") << 1;
        QTest::newRow("download") << 2;
    }

    void cancellation()
    {
        QFETCH(int, cancelRequest);
        QWidget window;
        TestNetwork network;
        network.delay = 100;
        network.metadata = QJsonDocument(fixture()).toJson();
        AppUpdater updater(&window, &network);
        QSignalSpy install(&updater, &AppUpdater::installRequested);
        bool cancelled = false;
        QTimer responder;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            if(auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            {
                for(QAbstractButton* button : box->buttons())
                    if(button->text() == "Download Update")
                    {
                        button->click();
                        return;
                    }
            }
            if(network.requests == cancelRequest)
            {
                if(auto dialog = qobject_cast<QProgressDialog*>(QApplication::activeModalWidget()))
                {
                    for(QPushButton* button : dialog->findChildren<QPushButton*>())
                        if(button->text() == "Cancel")
                        {
                            cancelled = true;
                            button->click();
                            return;
                        }
                }
            }
        });
        responder.start(10);
        updater.check();
        QTRY_VERIFY(cancelled);
        QTest::qWait(150);
        QCOMPARE(install.count(), 0);
        QCOMPARE(network.requests, cancelRequest);
    }

    void liveGithubCheck()
    {
        if(!qEnvironmentVariableIsSet("PM3_TEST_LIVE_UPDATE"))
            QSKIP("Set PM3_TEST_LIVE_UPDATE to test the real GitHub endpoint.");
        QApplication::setApplicationVersion("999.0.0");
        QWidget window;
        AppUpdater updater(&window);
        QString message;
        QTimer responder;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            if(auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            {
                message = box->text();
                box->grab().save("update-check-dialog.png");
                box->accept();
            }
        });
        responder.start(20);
        updater.check();
        QTRY_VERIFY_WITH_TIMEOUT(!message.isEmpty(), 35000);
        QVERIFY2(message.contains("up to date"), qPrintable(message));
        QApplication::setApplicationVersion("0.3.3");
    }

    void liveGithubDownload()
    {
        if(!qEnvironmentVariableIsSet("PM3_TEST_LIVE_DOWNLOAD"))
            QSKIP("Set PM3_TEST_LIVE_DOWNLOAD to download and verify the real installer without launching it.");
        QApplication::setApplicationVersion("0.0.0");
        QWidget window;
        AppUpdater updater(&window);
        QSignalSpy install(&updater, &AppUpdater::installRequested);
        QString error;
        QTimer responder;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            if(auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            {
                for(QAbstractButton* button : box->buttons())
                    if(button->text() == "Download Update")
                    {
                        button->click();
                        return;
                    }
                error = box->text();
                box->accept();
            }
        });
        responder.start(20);
        updater.check();
        QTRY_VERIFY_WITH_TIMEOUT(install.count() == 1 || !error.isEmpty(), 120000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(install.count(), 1);
        QApplication::setApplicationVersion("0.3.3");
    }

    void upToDateAndNetworkError()
    {
        QFETCH(int, status);
        QApplication::setApplicationVersion("0.4.0");
        QWidget window;
        TestNetwork network;
        network.metadata = QJsonDocument(fixture()).toJson();
        network.metadataStatus = status;
        AppUpdater updater(&window, &network);
        QSignalSpy install(&updater, &AppUpdater::installRequested);
        QString message;
        QTimer responder;
        connect(&responder, &QTimer::timeout, &window, [&]() {
            auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(box)
            {
                message = box->text();
                box->accept();
            }
        });
        responder.start(10);
        updater.check();
        QTRY_VERIFY(!message.isEmpty());
        QVERIFY(message.contains(status == 200 ? "up to date" : "Could not check"));
        QCOMPARE(install.count(), 0);
        QCOMPARE(network.requests, 1);
        QApplication::setApplicationVersion("0.3.3");
    }

    void versionComparison()
    {
        const auto release = AppUpdateInfo::parse(fixture());
        QCOMPARE(release.version.toString(), QString("0.4.0"));
        QCOMPARE(release.notes, QString("Release notes"));
        QVERIFY(!release.downloadUrl.isEmpty());
        QVERIFY(QVersionNumber::compare(release.version, QVersionNumber(0, 3, 3)) > 0);
        QVERIFY(QVersionNumber::compare(QVersionNumber(0, 3, 10), QVersionNumber(0, 3, 9)) > 0);
        QCOMPARE(QVersionNumber::compare(release.version, QVersionNumber(0, 4, 0)), 0);
        auto json = fixture();
        json["prerelease"] = true;
        QVERIFY(AppUpdateInfo::parse(json).version.isNull());
        json = fixture();
        json["draft"] = true;
        QVERIFY(AppUpdateInfo::parse(json).version.isNull());
        json = fixture();
        json["tag_name"] = "v0.4.0-beta";
        QVERIFY(AppUpdateInfo::parse(json).version.isNull());
    }

    void untrustedAssets()
    {
        const auto original = fixture();
        const auto originalAsset = original["assets"].toArray()[0].toObject();
        for(const QString& url : {QString("https://example.com/setup.exe"),
                                 QString("http://github.com/unicastbg/Proxmark3GUI-Modern/releases/download/v0.4.0/Proxmark3GUI-Modern-0.4.0-setup.exe"),
                                 QString("https://github.com/other/repository/releases/download/v0.4.0/Proxmark3GUI-Modern-0.4.0-setup.exe")})
        {
            auto json = original;
            auto asset = originalAsset;
            asset["browser_download_url"] = url;
            json["assets"] = QJsonArray {asset};
            QVERIFY(AppUpdateInfo::parse(json).downloadUrl.isEmpty());
        }
        for(const QString& digest : {QString(), QString("sha256:bad"), QString("md5:1234")})
        {
            auto json = original;
            auto asset = originalAsset;
            asset["digest"] = digest;
            json["assets"] = QJsonArray {asset};
            QVERIFY(AppUpdateInfo::parse(json).downloadUrl.isEmpty());
        }
        for(double size : {0.0, -1.0, 3.5, double(AppUpdateInfo::MaxInstallerSize + 1)})
        {
            auto json = original;
            auto asset = originalAsset;
            asset["size"] = size;
            json["assets"] = QJsonArray {asset};
            QVERIFY(AppUpdateInfo::parse(json).downloadUrl.isEmpty());
        }
    }

    void redirectValidation()
    {
        QVERIFY(AppUpdateInfo::allowedDownloadUrl(QUrl("https://release-assets.githubusercontent.com/signed-installer?token=example")));
        QVERIFY(AppUpdateInfo::allowedDownloadUrl(QUrl("https://objects.githubusercontent.com/installer")));
        QVERIFY(!AppUpdateInfo::allowedDownloadUrl(QUrl("http://release-assets.githubusercontent.com/installer")));
        QVERIFY(!AppUpdateInfo::allowedDownloadUrl(QUrl("https://release-assets.githubusercontent.com.evil.example/installer")));
        QVERIFY(!AppUpdateInfo::allowedDownloadUrl(QUrl("https://user:pass@release-assets.githubusercontent.com/installer")));
        QVERIFY(!AppUpdateInfo::allowedDownloadUrl(QUrl("https://release-assets.githubusercontent.com:8443/installer")));
        QVERIFY(!AppUpdateInfo::allowedDownloadUrl(QUrl("file:///C:/setup.exe")));
    }

    void installerIntegrity()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("setup.exe"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("abc"), qint64(3));
        file.close();
        const auto release = AppUpdateInfo::parse(fixture());
        QVERIFY(AppUpdateInfo::verifyInstaller(file.fileName(), release));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("abd"), qint64(3));
        file.close();
        QVERIFY(!AppUpdateInfo::verifyInstaller(file.fileName(), release));
        QVERIFY(file.open(QIODevice::Append));
        file.write("extra");
        file.close();
        QVERIFY(!AppUpdateInfo::verifyInstaller(file.fileName(), release));
        QVERIFY(!AppUpdateInfo::verifyInstaller(dir.filePath("missing.exe"), release));
    }
};

QTEST_MAIN(AppUpdateInfoTest)
#include "tst_appupdateinfo.moc"
