#ifndef APPUPDATEINFO_H
#define APPUPDATEINFO_H

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>

namespace AppUpdateInfo
{
const qint64 MaxInstallerSize = 512LL * 1024 * 1024;

struct Release
{
    QVersionNumber version;
    QString notes;
    QUrl downloadUrl;
    QByteArray sha256;
    qint64 size = 0;
};

inline bool allowedDownloadUrl(const QUrl& url)
{
    if(url.scheme() != "https" || !url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443))
        return false;
    if(url.host() == "github.com")
        return url.path().startsWith("/unicastbg/Proxmark3GUI-Modern/releases/download/");
    return url.host() == "release-assets.githubusercontent.com"
        || url.host() == "objects.githubusercontent.com";
}

inline Release parse(const QJsonObject& json)
{
    Release release;
    const QString tag = json.value("tag_name").toString();
    if(json.value("draft").toBool() || json.value("prerelease").toBool()
       || !QRegularExpression("^v?[0-9]+\\.[0-9]+\\.[0-9]+$").match(tag).hasMatch())
        return release;
    const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
    release.version = QVersionNumber::fromString(version);
    release.notes = json.value("body").toString().left(30000);
    const QString filename = "Proxmark3GUI-Modern-" + version + "-setup.exe";
    const QUrl expectedUrl("https://github.com/unicastbg/Proxmark3GUI-Modern/releases/download/" + tag + "/" + filename);
    for(const QJsonValue& value : json.value("assets").toArray())
    {
        const QJsonObject asset = value.toObject();
        if(asset.value("name").toString() != filename)
            continue;
        const QString digest = asset.value("digest").toString();
        const double size = asset.value("size").toDouble();
        if(QUrl(asset.value("browser_download_url").toString()) != expectedUrl
           || !QRegularExpression("^sha256:[0-9A-Fa-f]{64}$").match(digest).hasMatch()
           || size <= 0 || size > MaxInstallerSize || size != static_cast<qint64>(size))
            continue;
        release.downloadUrl = expectedUrl;
        release.sha256 = digest.mid(7).toLatin1().toLower();
        release.size = static_cast<qint64>(size);
        break;
    }
    return release;
}

inline bool verifyInstaller(const QString& path, const Release& release)
{
    QFile file(path);
    if(release.sha256.size() != 64 || release.size <= 0 || release.size > MaxInstallerSize
       || !file.open(QIODevice::ReadOnly) || file.size() != release.size)
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == release.sha256;
}
}

#endif
