#include "PreviewWidget.h"

#include <QtCompilerDetection>
#include <QDebug>
#include <QSharedData>
#include <QFile>
#include <QDir>
#include <QStringView>
#include <QString>
#include <QCryptographicHash>
#include <QApplication>

#include <QPainter>
#include <QImage>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget{parent}
{

}

PreviewWidget::~PreviewWidget()
{
    qDebug()<<Q_FUNC_INFO<<"----------------";
}

bool PreviewWidget::load(const QString &jsonPath, const QString &mediaPath)
{
    if (jsonPath.isEmpty() || !QFile::exists(jsonPath)) {
        qDebug()<<Q_FUNC_INFO<<"Invalid json file "<<jsonPath;
        return false;
    }
    if (mediaPath.isEmpty()) {
        // Q_EMIT dlError(QLatin1StringView("Empty out path!"));
        return false;
    }
    QDir dir(mediaPath);
    if (!dir.exists()) {
        // Q_EMIT dlError(QLatin1StringView("Error to create path [%1]!").arg(dataFile));
        return false;
    }
    m_mediaPath = mediaPath;

    //Copy from MediaDownloader::download
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // Q_EMIT dlError(QLatin1StringView("Can't open as readonly for [%1]!").arg(dataFile));
        return false;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        // Q_EMIT dlError(QLatin1StringView("parse json error at offset [%1]!").arg(QString::number(error.offset)));
        return false;
    }

    auto root = doc.object();
    auto data = root.value("data").toObject();
    if (data.isEmpty()) {
        // Q_EMIT dlError((QLatin1StringView("Parse 'data' node error!")));
        return false;
    }

    {
        if (auto Property = data.value("Property").toObject(); !Property.isEmpty()) {
            if (!parseProperty(Property)) {
                qDebug()<<Q_FUNC_INFO<<"parese Property Error: "<<Property;
                return false;
            }
        } else {
            qDebug()<<Q_FUNC_INFO<<"get Property object error";
            return false;
        }
    }

    if (m_pages = data.value("Pages").toArray(); m_pages.isEmpty()) {
        qDebug()<<Q_FUNC_INFO<<"can't find pages";
        return false;
    }

    if (!m_sceneImg) {
        m_sceneImg = new QImage(m_pageSize.PageWidth, m_pageSize.PageHeight, QImage::Format_ARGB32);
        m_sceneImg->fill(Qt::GlobalColor::magenta);
    }
    if (!m_scenePainter) {
        m_scenePainter = new QPainter(m_sceneImg);
    }

    return true;
}

void PreviewWidget::drawPage(int pgNum)
{
    if (pgNum > m_pages.size()) {
        qDebug()<<Q_FUNC_INFO<<"Invalid pgNum "<<pgNum<<", total size "<<m_pages.size();
        return;
    }
    // m_curID = pgNum;
    auto root = m_pages.at(pgNum).toObject();
    m_curID = root.value("ID").toInt(-1);
    drawBackground(root);

}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
    if (!m_sceneImg) {
        QWidget::paintEvent(event);
        return;
    }

    QPainter p;
    p.begin(this);
    p.drawImage(this->rect().topLeft(), *m_sceneImg);
    p.end();
}

bool PreviewWidget::parseProperty(const QJsonObject &obj)
{
    if (obj.isEmpty()) {
        return false;
    }
    if (auto PageSize = obj.value("PageSize").toObject(); !PageSize.isEmpty()) {
        m_pageSize.PageHeight       = PageSize.value("PageHeight").toInt();
        m_pageSize.PageWidth        = PageSize.value("PageWidth").toInt();
        m_pageSize.FeedPageHeight   = PageSize.value("FeedPageHeight").toInt();
        m_pageSize.FeedPageWidth    = PageSize.value("FeedPageWidth").toInt();
        m_pageSize.SubjectPageWidth = PageSize.value("SubjectPageWidth").toInt();
        return true;
    }
    return false;
}

//TODO refactor
auto tag = [](const QString &uri) -> QString {
    if (int idx = uri.lastIndexOf("."); idx >=0) {
        return uri.sliced(idx+1);
    }
    return QString();
};

#define GET_FILE(uri) \
    QString("%1/%2/%3.%4") \
        .arg(m_mediaPath) \
        .arg(m_curID) \
        /*.arg(obj.uri().toUtf8().toBase64())*/ \
        .arg(QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex()) \
        .arg(tag(uri));


void PreviewWidget::drawBackground(const QJsonObject &rootNode)
{
    if (auto Property = rootNode.value("Property").toObject(); !Property.isEmpty()) {
        if (auto Background = Property.value("Background").toObject(); !Background.isEmpty()) {
            auto uri = Background.value("ImageUrl").toString();
            auto fname = GET_FILE(uri);
            QImage img;
            if (img.load(fname)) {
                m_scenePainter->drawImage(m_sceneImg->rect().topLeft(), img);
                this->update();
            }
        }
    }

}















