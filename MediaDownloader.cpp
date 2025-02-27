#include "MediaDownloader.h"

#include <QtCompilerDetection>
#include <QDebug>
#include <QSharedData>
#include <QFile>
#include <QDir>
#include <QStringView>
#include <QString>
#include <QCryptographicHash>
#include <QApplication>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "YQZDGlobal.h"

const static int DL_MAX_CNT = 5;

class MediaObjectPriv : public QSharedData
{
public:
    MediaObjectPriv()
        : id(-2)
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
    if (!m_replyList.isEmpty()) {
        for (const auto it : m_replyList) {
            if (it->isRunning()) {
                it->abort();
            }
            it->deleteLater();
        }
        m_replyList.clear();
    }
    m_networkMgr->deleteLater();;
}

void MediaDownloader::download(const QString &dataFile, const QString &outPath)
{
    if (dataFile.isEmpty() || !QFile::exists(dataFile)) {
        Q_EMIT dlError(QString("Data file [%1] not exist!").arg(dataFile));
        return;
    }
    if (outPath.isEmpty()) {
        Q_EMIT dlError(QLatin1StringView("Empty out path!"));
        return;
    }
    QDir dir(outPath);
    if (!dir.exists() && !dir.mkpath(outPath)) {
        Q_EMIT dlError(QLatin1StringView("Error to create path [%1]!").arg(dataFile));
        return;
    }

    QFile file(dataFile);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT dlError(QLatin1StringView("Can't open as readonly for [%1]!").arg(dataFile));
        return;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        Q_EMIT dlError(QLatin1StringView("parse json error at offset [%1]!").arg(QString::number(error.offset)));
        return;
    }

    auto root = doc.object();
    auto data = root.value("data").toObject();
    if (data.isEmpty()) {
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

    qDebug()<<Q_FUNC_INFO<<">>>>>>> found  pages, number: "<<pages.size();

    for (const auto &page : pages) {
        auto obj = page.toObject();
        if (obj.isEmpty()) {
            qDebug()<<Q_FUNC_INFO<<"Ingore invalid page object "<<page;
            continue;
        }
        const int id = obj.value("ID").toInt(-1);
        if (id == -1) {
            qDebug()<<Q_FUNC_INFO<<"Ignore as invalid id for "<<obj;
            continue;
        }
        // Element is an object
        if (auto Element = obj.value("Element").toObject(); !Element.isEmpty()) {
            CHK_AND_APPEND(Element, "Logo", id);
            CHK_AND_APPEND(Element, "OrginURL", id);
            if (auto Media = Element.value("Media").toObject(); !Media.isEmpty()) {
                CHK_AND_APPEND(Media, "URL", id);
            }
            if (auto GraduationAudios = Element.value("GraduationAudios").toArray(); !GraduationAudios.isEmpty()) {
                for (auto ga : GraduationAudios) {
                    auto gaobj = ga.toObject();
                    if (gaobj.isEmpty()) {
                        qDebug()<<Q_FUNC_INFO<<"Ignore current Element:{GraduationAudios:[]} object: "<<ga;
                        continue;
                    }
                    CHK_AND_APPEND(gaobj, "AvatarURL", id);
                    CHK_AND_APPEND(gaobj, "OriginAudioURL", id);
                }
            }
            if (auto Image = Element.value("Image").toObject(); !Image.isEmpty()) {
                CHK_AND_APPEND(Image, "URL", id);
            }
            if (const auto Images = Element.value("Images").toArray(); !Images.isEmpty()) {
                for (const auto &it : Images) {
                    if (const auto o = it.toObject(); !o.isEmpty()) {
                        CHK_AND_APPEND(o, "URL", id);
                    }
                }
            }


        }
        if (auto Property = obj.value("Property").toObject(); !Property.isEmpty()) {
            if (auto Background = Property.value("Background").toObject(); !Background.isEmpty()) {
                CHK_AND_APPEND(Background, "ImageUrl", id);
            }
        }
        //ElementS is an array
        if (auto Elements = obj.value("Elements").toArray(); !Elements.isEmpty()) {
            for (const auto &ele : Elements) {
                auto eleObj = ele.toObject();
                if (eleObj.isEmpty()) {
                    qDebug()<<Q_FUNC_INFO<<"Ingore as current object in Elements is not object: "<<ele;
                    continue;
                }
                if (auto Body = eleObj.value("Body").toObject(); !Body.isEmpty()) {
                    //media in body object => for image type
                    if (auto Media = Body.value("Media").toObject(); !Media.isEmpty()) {
                        //other Elements in Meida
                        if (auto MediaElements = Media.value("Elements").toArray(); !MediaElements.isEmpty()) {
                            for (const auto &md : MediaElements) {
                                auto mdObj = md.toObject();
                                if(mdObj.isEmpty()) {
                                    qDebug()<<Q_FUNC_INFO<<"Ignore current Media{Elements:[]} object "<<mdObj;
                                    continue;
                                }
                                CHK_AND_APPEND(mdObj, "URL", id);
                            }
                        }
                    }
                    //Video in body object => for video and QR code
                    if (auto Video = Body.value("Video").toObject(); !Video.isEmpty()) {
                        if (auto Image = Video.value("Image").toObject(); !Image.isEmpty()) {
                            CHK_AND_APPEND(Image, "URL", id);
                            CHK_AND_APPEND(Image, "VideoUri", id);
                        }
                        //NOTE ignore QRcode url as same as VideoUri
                    }
                    if (auto Template = Body.value("Template").toObject(); !Template.isEmpty()) {
                        if (auto Images = Template.value("Images").toArray(); !Images.isEmpty()) {
                            for (const auto &img : Images) {
                                auto imgObj = img.toObject();
                                if (imgObj.isEmpty()) {
                                    qDebug()<<Q_FUNC_INFO<<"Ignore current Template{Images:[]} object "<<imgObj;
                                    continue;
                                }
                                CHK_AND_APPEND(imgObj, "URL", id);
                            }
                        }
                    }
                }
            }
        } //end ElementS is an array
    }

    qDebug()<<Q_FUNC_INFO<<">>>>>>> final download data size : "<<m_dlList.size();
    for (const auto &o : m_dlList) {
        qDebug()<<"ID ["<<o.id()
                 <<"], path ["<<o.path()
                 <<"], url ["<<o.uri()
                 <<"]";
    }

    processDownload();

    Q_EMIT downloadState(QLatin1StringView("Current download finish"));
}

static bool flag = true;
void MediaDownloader::processDownload()
{
    while (!m_dlList.isEmpty()) {
        if (m_workingMap.size() == DL_MAX_CNT) {
            qApp->processEvents();
            continue;
        }

        auto obj = m_dlList.takeFirst();
        m_workingMap.insert(obj.uri(), obj);

        auto reply = m_networkMgr->get(QNetworkRequest(obj.uri()));
        m_replyList.append(reply);

        connect(reply, &QNetworkReply::finished,
                this, [=]() {
                    m_replyList.removeOne(reply);
                    auto obj = m_workingMap.take(reply->url().toString());

                    qDebug()<<"reply ID ["<<obj.id()
                             <<"], path ["<<obj.path()
                             <<"], url ["<<obj.uri()
                             <<"], reply url string ["<<reply->url().toString()
                             <<"]";

                    if (reply->error() != QNetworkReply::NoError) {
                        qDebug()<<Q_FUNC_INFO<<"download error "<<reply->errorString();
                        reply->deleteLater();

                        m_dlList.append(obj);
                        if (!flag) {
                            qDebug()<<Q_FUNC_INFO<<"---- restart download for failure object ";
                            Q_EMIT downloadState(QString("Re-stared failure obj %1").arg(obj.uri()));
                            processDownload();
                        }
                        return;
                    }
#if (MEDIA_PATH_SEPARATE_BY_ID)
                    QDir dir(QString("%1/%2").arg(obj.path()).arg(obj.id()));
                    if (!dir.exists() && !dir.mkpath(QString("%1/%2").arg(obj.path()).arg(obj.id()))) {
#else
                    QDir dir(obj.path());
                    if (!dir.exists() && !dir.mkpath(obj.path())) {
#endif
                        qDebug()<<Q_FUNC_INFO<<"mk dir error";
                        reply->deleteLater();
                        return;
                    }
                    auto tag = [](const QString &uri) -> QString {
                        if (int idx = uri.lastIndexOf("."); idx >=0) {
                            return uri.sliced(idx+1);
                        }
                        return QString();
                    };
#if (MEDIA_PATH_SEPARATE_BY_ID)
                    auto fName = QString("%1/%2/%3.%4")
                                     .arg(obj.path())
                                     .arg(obj.id())
                                     // .arg(obj.uri().toUtf8().toBase64())
                                     .arg(QCryptographicHash::hash(obj.uri().toUtf8(), QCryptographicHash::Md5).toHex())
                                     .arg(tag(obj.uri()));
#else
                    auto fName = QString("%1/%2.%3")
                                     .arg(obj.path())
                                     .arg(QCryptographicHash::hash(obj.uri().toUtf8(), QCryptographicHash::Md5).toHex())
                                     .arg(tag(obj.uri()));
#endif

                    qDebug()<<Q_FUNC_INFO<<"save to "<<fName;

                    QFile f(fName);
                    if (!f.open(QIODevice::WriteOnly)) {
                        qDebug()<<Q_FUNC_INFO<<"open error";
                        f.close();
                        reply->deleteLater();
                        return;
                    }
                    f.write(reply->readAll());
                    f.flush();
                    f.close();
                });
    }
    flag = !flag;
}




