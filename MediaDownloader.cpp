#include "MediaDownloader.h"

#include <QtCompilerDetection>
#include <QDebug>
#include <QSharedData>
#include <QFile>
#include <QDir>
#include <QStringView>
#include <QString>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

class MediaObjectPriv : public QSharedData
{
public:
    MediaObjectPriv()
        : id(-1)
    {

    }
    ~MediaObjectPriv()
    {

    }

    int id;
    QString uri;
    QString path;
};


MediaObject::MediaObject()
    : d(new MediaObjectPriv)
{

}


MediaObject::MediaObject(const MediaObject &other)
    : d(other.d)
{

}

MediaObject &MediaObject::operator=(const MediaObject &other)
{
    if (this != &other) {
        d.operator =(other.d);
    }
    return *this;
}

bool MediaObject::operator ==(const MediaObject &other)
{
    return d.data()->id         == other.d->id
           && d.data()->path    == other.d->path
           && d.data()->uri     == other.d->uri;

}

bool MediaObject::operator !=(const MediaObject &other)
{
    return !operator==(other);
}

MediaObject::~MediaObject()
{

}

QString MediaObject::uri() const
{
    return d->uri;
}

void MediaObject::setUri(const QString &newUri)
{
    d->uri = newUri;
}

int MediaObject::id() const
{
    return d->id;
}

void MediaObject::setId(int newId)
{
    d->id = newId;
}

QString MediaObject::path() const
{
    return d->path;
}

void MediaObject::setPath(const QString &path)
{
    d->path = path;
}


/********************************************************
 *
 * *****************************************************/

MediaDownloader::MediaDownloader(QObject *parent)
    : QObject(parent)
    , m_networkMgr(new QNetworkAccessManager(this))
{

}

MediaDownloader::~MediaDownloader()
{

}

void MediaDownloader::download(const QString &dataFile, const QString &outPath)
{
    if (dataFile.isEmpty() || !QFile::exists(dataFile)) {
        Q_EMIT dlError(QString("Data file [%1] not exist!").arg(dataFile));
        return;
    }
    QDir dir(outPath);
    if (!dir.exists() && !dir.mkpath(outPath)) {
        Q_EMIT dlError(QString("Error to create path [%1]!").arg(dataFile));
        return;
    }

    QFile file(dataFile);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT dlError(QString("Can't open as readonly for [%1]!").arg(dataFile));
        return;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        Q_EMIT dlError(QString("parse json error at offset [%1]!").arg(error.offset));
        return;
    }

    auto root = doc.object();
    auto data = root.value("data").toObject();
    if (!data.isEmpty()) {
        Q_EMIT dlError((QLatin1StringView("Parse 'data' node error!")));
        return;
    }

#define APPEND_OBJ(id, str) \
    do { \
            MediaObject obj; \
            obj.setId(id); \
            obj.setPath(outPath); \
            obj.setUri(str); \
            m_dlList.append(obj); \
    } while(0);

#define CHK_AND_APPEND(root, key, id) \
    do { \
            auto str = root.value(key).toString(); \
            if (!str.isNull() && !str.isEmpty()) { \
                APPEND_OBJ(id, str); \
        } \
    } while(0);


    {
        auto profile = data.value("Profile").toObject();
        if (profile.isEmpty()) {
            qDebug()<<Q_FUNC_INFO<<"Parse 'profile' node error";
        } else {
            CHK_AND_APPEND(profile, "Avatar", -1);
            CHK_AND_APPEND(profile, "Cover", -1);
            CHK_AND_APPEND(profile, "Backcover", -1);
        }
    }

    auto pages = data.value("Pages").toArray();
    if (pages.isEmpty()) {
        Q_EMIT dlError((QLatin1StringView("No pages found!!")));
        return;
    }














}




