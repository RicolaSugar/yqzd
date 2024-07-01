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
#include <QtNumeric>

#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QPixmap>
#include <QFontMetrics>

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

#define GET_FILE(uri) \
QString("%1/%2/%3.%4") \
    .arg(m_mediaPath) \
    .arg(m_curID) \
    /*.arg(obj.uri().toUtf8().toBase64())*/ \
    .arg(QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex()) \
    .arg(dotExtension(uri));

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

    if (auto Property = data.value("Property").toObject(); !Property.isEmpty()) {
        if (!parseProperty(Property)) {
            qDebug()<<Q_FUNC_INFO<<"parese Property Error: "<<Property;
            return false;
        }
    } else {
        qDebug()<<Q_FUNC_INFO<<"get Property object error";
        return false;
    }

    if (auto Profile = data.value("Profile").toObject(); !Profile.isEmpty()) {
        if (const QString Avatar = Profile.value("Avatar").toString(); !Avatar.isEmpty()) {
            m_profileAvatar = GET_FILE(Avatar);
            if (!QFile::exists(m_profileAvatar)) {
                qWarning()<<Q_FUNC_INFO<<"Can't find ProfileAvatar in path "<<m_profileAvatar;
                m_profileAvatar = QString();
            }
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

    if (auto Property = root.value("Property").toObject(); !Property.isEmpty()) {
        drawBackground(Property);

        const auto Type = Property.value("Type").toString();

        qDebug()<<Q_FUNC_INFO<<"type "<<Type;

        if (Type == QLatin1StringView("intro")) {
            drawIntroPage(root);
        }
        else if (Type == QLatin1StringView("version")) {
            drawVersionPage(root);
        }
        else if (Type == QLatin1StringView("directory")) {
            drawDirectoryPage(root);
        }
        else if (Type == QLatin1StringView("profile")) {
            drawProfilePage(root);
        }
    }




    // drawBackground(root);

    // if (auto ele = root.value("Element").toObject(); !ele.isEmpty()) {
    //     drawElement(ele);
    // }



    this->update();

}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
    if (!m_sceneImg) {
        QWidget::paintEvent(event);
        return;
    }

    QPainter p;
    p.begin(this);
    p.drawImage(this->rect().topLeft(), m_sceneImg->scaled(this->rect().size(),
                                                           Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation));
    p.end();
}

bool PreviewWidget::parseProperty(const QJsonObject &obj)
{
    if (obj.isEmpty()) {
        return false;
    }
    if (const auto PageSize = obj.value("PageSize").toObject(); !PageSize.isEmpty()) {
        m_pageSize.PageHeight       = PageSize.value("PageHeight").toInt();
        m_pageSize.PageWidth        = PageSize.value("PageWidth").toInt();
        m_pageSize.FeedPageHeight   = PageSize.value("FeedPageHeight").toInt();
        m_pageSize.FeedPageWidth    = PageSize.value("FeedPageWidth").toInt();
        m_pageSize.SubjectPageWidth = PageSize.value("SubjectPageWidth").toInt();
     }
    if (const auto DividingLine = obj.value("DividingLine").toObject(); !DividingLine.isEmpty()) {
        m_dvLine.X      = DividingLine.value("X").toInt();
        m_dvLine.Width  = DividingLine.value("Width").toInt();
        m_dvLine.Height = DividingLine.value("Height").toInt();
        m_dvLine.Color  = DividingLine.value("Color").toString();
    }

    return true;
}

void PreviewWidget::drawIntroPage(const QJsonObject &node)
{
    if (auto ele = node.value("Element").toObject(); !ele.isEmpty()) {
        // drawElement(ele);
        if (const int TemplateType = ele.value("TemplateType").toInt(); TemplateType == 1) {
            drawTemplateElement(ele);
            return;
        }
    }
}

void PreviewWidget::drawVersionPage(const QJsonObject &node)
{

    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        // m_scenePainter->restore();
        const int xpos = (m_pageSize.PageWidth - m_pageSize.FeedPageWidth) /2;
        //TODO magic code for x/y space
        const int yspace = 96;
        const int xspace = 96;
        //TODO magic code for verison page start y pos;
        int ypos = m_pageSize.PageHeight *3/10;
        if (const auto Head = Element.value("Head").toObject(); !Head.isEmpty()) {
            const auto Headline = Head.value("Headline").toString();
            const auto Subline = Head.value("Subline").toString();

            auto font = m_scenePainter->font();
            //TODO magic code for font size
            font.setPixelSize(112);
            m_scenePainter->setFont(font);
            m_scenePainter->drawText(xpos, ypos, Headline);

            QFontMetrics fm(font);
            auto w = fm.horizontalAdvance(Headline);
            w += xspace;
            //TODO magic code for font size
            font.setPixelSize(60);
            m_scenePainter->setFont(font);
            m_scenePainter->drawText(xpos + w, ypos, Subline);
        }
        if (const auto Body = Element.value("Body").toObject(); !Body.isEmpty()) {
            ypos += yspace;
            m_scenePainter->setBrush(QColor::fromString(m_dvLine.Color));
            m_scenePainter->drawLine(xpos, ypos,
                                     m_pageSize.PageWidth - xpos, ypos);

            ypos += yspace;
            auto font = m_scenePainter->font();
             //TODO magic code for font size
            font.setPixelSize(48);
            m_scenePainter->setFont(font);
            QFontMetrics fm(font);

            if (const auto Authors = Body.value("Authors").toString(); !Authors.isEmpty()) {
                ypos += yspace;
                const QString cn_str("作者：");
                m_scenePainter->drawText(xpos, ypos, cn_str);
                auto w = fm.horizontalAdvance(cn_str);
                m_scenePainter->drawText(xpos + w, ypos, Authors);
            }

            if (const auto PageNumber = Body.value("PageNumber").toInt(-1); PageNumber != -1) {
                ypos += yspace;
                const QString cn_str("页数：");
                m_scenePainter->drawText(xpos, ypos, cn_str);
                auto w = fm.horizontalAdvance(cn_str);
                m_scenePainter->drawText(xpos + w, ypos, QString::number(PageNumber));
            }
            if (const auto Records = Body.value("Records").toString(); !Records.isEmpty()) {
                ypos += yspace;
                const QString cn_str("记录：");
                m_scenePainter->drawText(xpos, ypos, cn_str);
                auto w = fm.horizontalAdvance(cn_str);
                m_scenePainter->drawText(xpos + w, ypos, Records);
            }
            if (const auto TimeInterval = Body.value("TimeInterval").toString(); !TimeInterval.isEmpty()) {
                ypos += yspace;
                const QString cn_str("时间：");
                m_scenePainter->drawText(xpos, ypos, cn_str);
                auto w = fm.horizontalAdvance(cn_str);
                m_scenePainter->drawText(xpos + w, ypos, TimeInterval);
            }
        }

    }

}

void PreviewWidget::drawDirectoryPage(const QJsonObject &node)
{
    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        if (const auto Entries = Element.value("Entries").toArray(); !Entries.isEmpty()) {
            const int xpos      = (m_pageSize.PageWidth - m_pageSize.FeedPageWidth) /2;
            //TODO magic code for x/y space
            const int xspace    = 96;
            for (const auto &it : Entries) {
                if (const auto obj = it.toObject(); !obj.isEmpty()) {
                    auto font       = m_scenePainter->font();
                    const int Type  = obj.value("Type").toInt(-1);
                    //TODO magic code for font size
                    font.setPixelSize(48);
                    if (Type == 1) {
                        font.setPixelSize(72);
                    } else if (Type == 2) {
                        font.setPixelSize(48);
                    }
                    if (Type != -1) {
                        m_scenePainter->setFont(font);
                    }

                    const int ypos          = obj.value("Y").toInt();
                    const int Pagination    = obj.value("Pagination").toInt();
                    const bool HasVideo     = obj.value("HasVideo").toBool();
                    //TODO use emoji?
                    const auto Text         = obj.value("Text").toString() + (HasVideo ? "  \u231B" : "");

                    if (Pagination < 100) {
                        auto ptext = QString("%1").arg(Pagination, 2, 10, QChar('0'));
                        m_scenePainter->drawText(xpos, ypos, ptext);
                    } else {
                        m_scenePainter->drawText(xpos, ypos, QString::number(Pagination));
                    }
                    m_scenePainter->drawText(xpos + xspace * 3, ypos, Text);
                }
            }
        }
    }
}

void PreviewWidget::drawProfilePage(const QJsonObject &node)
{
    //TODO magic code for pos and size
    //圆形头像 直径530,x455, y685
    //name/age x1150, y940
    //KindergartenName x560, y1410
    //Teachers, y2035
    //Hobbies, y2710

    if (QImage profileAvatar; profileAvatar.load(m_profileAvatar)) {
        profileAvatar = profileAvatar.scaled(530, 530, Qt::KeepAspectRatio);

        QPixmap pm(530, 530);
        pm.fill(Qt::GlobalColor::transparent);

        QPainter p(&pm);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        QPainterPath path;
        path.addEllipse(0, 0, pm.width(), pm.height());
        p.setClipPath(path);
        p.drawImage(pm.rect(), profileAvatar);
        m_scenePainter->drawPixmap(455, 685, pm);
    }

    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {

    }
}

void PreviewWidget::drawBackground(const QJsonObject &PropertyObject)
{
    // if (auto Property = PropertyObject.value("Property").toObject(); !Property.isEmpty()) {
        int Height= PropertyObject.value("Height").toInt();
        qDebug()<<Q_FUNC_INFO<<"Height "<<Height;
        if (auto Background = PropertyObject.value("Background").toObject(); !Background.isEmpty()) {
            auto uri = Background.value("ImageUrl").toString();
            auto fname = GET_FILE(uri);
            QImage img;
            if (img.load(fname)) {
                //TODO fit size
                img = img.scaledToHeight(Height, Qt::SmoothTransformation);
                m_scenePainter->drawImage(m_sceneImg->rect().topLeft(),
                                          img,
                                          QRect(qMax(qAbs(img.width()-m_sceneImg->width()), 0),
                                                qMax(qAbs(img.height()-m_sceneImg->height()), 0),
                                                img.width(),
                                                img.height()));
            }
        }
    // }

}

void PreviewWidget::drawElement(const QJsonObject &node)
{
    // if (node.isEmpty()) {
    //     return;
    // }

    // if (const int TemplateType = node.value("TemplateType").toInt(); TemplateType == 1) {
    //     drawTemplateElement(node);
    //     return;
    // }
}

void PreviewWidget::drawTemplateElement(const QJsonObject &node)
{
    if (node.isEmpty()) {
        return;
    }
    /*
     * Meida image ypos 13% of height, xpos 14% of width
     * Logo/text ypos 94.5% of height
     */

    //xpos for image and text
    int xpos = m_sceneImg->width() *14/100;
    int width = m_sceneImg->width() * (100 - 14*2)/100;

    if (auto Media = node.value("Media").toObject(); !Media.isEmpty()) {
        auto uri = Media.value("URL").toString();
        auto fname = GET_FILE(uri);
        if (!QFile::exists(fname)) {
            qDebug()<<Q_FUNC_INFO<<"can't find local image "<<fname;
        } else {
            width = Media.value("WPixel").toInt();
            int height = Media.value("HPixel").toInt();
            xpos = (m_sceneImg->width() - width) /2;
            //TODO 13% from phone app screen capture
            int ypos = m_sceneImg->height() * 13/100;
            if (QImage img; img.load(fname)) {
                img = img.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                m_scenePainter->drawImage(QPoint(xpos, ypos), img);
            }
        }
    }
    /*
     * Text ypos 50% of height, from phone app screen capture
     * 30% height of screen height, from from phone app screen capture
     */
    if (const QString text = node.value("Text").toString(); !text.isEmpty()) {
        auto font = m_scenePainter->font();
        //TODO mageic size of font
        font.setPixelSize(56);
        m_scenePainter->setFont(font);

       m_scenePainter->drawText(xpos, m_sceneImg->height() /2,
                                 width, m_sceneImg->height() *30/100,
                                 Qt::TextWordWrap | Qt::TextIncludeTrailingSpaces,
                                 text);
    }

    int logoTextW = 0;
    auto flogo = GET_FILE(node.value("Logo").toString());
    QImage logoImg;
//TODO not correct for drawing logo image, remove atm
#if 0
    if (QFile::exists(flogo) && logoImg.load(flogo)) {
        //TODO mageic size of font * 2
        logoImg = logoImg.scaled(144, 144, Qt::KeepAspectRatio);
        logoTextW += logoImg.width();
        qDebug()<<Q_FUNC_INFO<<"logo image "<<logoImg;
    }
    //add space between logo and KindergartenName text
    //TODO magic size
    const int space = 62;
    logoTextW += space;
#else
    const int space = 0;
#endif
    auto KindergartenName = node.value("KindergartenName").toString();
    if (!KindergartenName.isEmpty()) {
        auto font = m_scenePainter->font();
        //TODO mageic size of font
        font.setPixelSize(72);
        m_scenePainter->setFont(font);

        QFontMetrics fm(font);
        logoTextW += fm.horizontalAdvance(KindergartenName);
    }
    xpos = (m_sceneImg->width() - logoTextW) /2;
    auto ypos = m_sceneImg->height() * 94/100;
#if 0
    if (!logoImg.isNull()) {
        //FIXME why ypos of logo image is not correct?
        m_scenePainter->drawImage(QPoint(xpos, ypos), logoImg);
    }
#endif
    if (!KindergartenName.isEmpty()) {
        m_scenePainter->drawText(QPoint(xpos + logoImg.width() + space, ypos), KindergartenName);
    }




}

QString PreviewWidget::dotExtension(const QString &uri)
{
    if (int idx = uri.lastIndexOf("."); idx >=0) {
        return uri.sliced(idx+1);
    }
    return QString();
}

















