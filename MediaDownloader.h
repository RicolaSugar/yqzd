#ifndef MEDIADOWNLOADER_H
#define MEDIADOWNLOADER_H


#include <QObject>
#include <QMultiMap>
#include <QSharedDataPointer>
#include <QNetworkAccessManager>

class MediaObjectPriv;
class MediaObject
{
public:
    MediaObject();
    MediaObject(const MediaObject &other);

    ~MediaObject();

    MediaObject &operator=(const MediaObject &other);
    bool operator ==(const MediaObject &other);
    bool operator !=(const MediaObject &other);

    QString uri() const;

    void setUri(const QString &newUri);

    int id() const;

    void setId(int newId);

    QString path() const;

    void setPath(const QString &path);

private:
     QSharedDataPointer<MediaObjectPriv> d;
};

class MediaDownloader : public QObject
{
    Q_OBJECT
public:
    explicit MediaDownloader(QObject *parent = nullptr);
    virtual ~MediaDownloader();

    void download(const QString &dataFile, const QString &outPath);


Q_SIGNALS:
    void dlError(const QString &errorMsg);
    void downloadState(const QString &msg);

private:
    void processDownload();

private:
    QNetworkAccessManager       *m_networkMgr = nullptr;
    QList<QNetworkReply*>       m_replyList;
    QList<MediaObject>          m_dlList;
    QMultiMap<QString, MediaObject>  m_workingMap;
};

#endif // MEDIADOWNLOADER_H
