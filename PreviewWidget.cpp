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
#include <QtMath>

#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QPixmap>
#include <QFontMetrics>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "PrivateURI.h"

#include "BarcodeFormat.h"
#include "BitMatrix.h"
#include "MultiFormatWriter.h"

#define FONT_YAHEI      QLatin1StringView("Microsoft YaHei")
#define FONT_YUANTI     QLatin1StringView("HYZhongYuanJ")
#define FONT_HAN_SANS   QLatin1StringView("Source Han Sans CN Normal")


#define GET_FILE(uri) \
QString("%1/%2/%3.%4") \
    .arg(m_mediaPath) \
    .arg(m_curID) \
    /*.arg(obj.uri().toUtf8().toBase64())*/ \
    .arg(QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex()) \
    .arg(dotExtension(uri))


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
        m_scenePainter->setRenderHints(QPainter::RenderHint::Antialiasing | QPainter::RenderHint::TextAntialiasing);
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
        else if (Type == QLatin1StringView("graduation-photo")) {
            drawGraduationPhotoPage(root);
        }
        else if (Type == QLatin1StringView("graduation-movie")) {
            drawGraduationMoviePaget(root);
        }
        else if (Type == QLatin1StringView("graduation-dream")) {
            drawGraduationDreamPage(root);
        }
        else if (Type == QLatin1StringView("hybrid-subject")) {
            drawHybridSubject(root, Property);
        }
        else if (Type == QLatin1StringView("feed")) {
            drawFeedPage(root, Property);
        }
        else if (Type == QLatin1StringView("subject")) {
            drawHybridSubject(root, Property);
        }
        else if (Type == QLatin1StringView("physical-examination")) {
            drawPhysicalExaminationPage(root, Property);
        }
        else if (Type == QLatin1StringView("e-wish")) {
            drawEWishPage(root, Property);
        }
        else if (Type == QLatin1StringView("graduation-audios")) {
            drawGraduationAudios(root, Property);
        }

    }

    if (const auto Pagination = root.value("Pagination").toObject(); !Pagination.isEmpty()) {
        drawPagination(Pagination);
    }




    // drawBackground(root);

    // if (auto ele = root.value("Element").toObject(); !ele.isEmpty()) {
    //     drawElement(ele);
    // }



    this->update();

}

int PreviewWidget::pageCount() const
{
    return m_pages.count();
}

QImage PreviewWidget::generateBarcode(const QString &text, int width, int height, QColor foreground, QColor background)
{
    auto format = ZXing::BarcodeFormatFromString("QRCode");

    //To draw image on QR Code use maximum level of ecc. Setting it to 8.
    auto writer = ZXing::MultiFormatWriter(format).setEccLevel(8);
    auto matrix = writer.encode(text.toStdString(), width, height);
    QImage img(width, height, QImage::Format_ARGB32);

    for (int y = 0; y < width; ++y) {
        for (int x = 0; x < height; ++x) {
            if (matrix.get(x, y)) {
                img.setPixelColor(x, y, foreground);
            } else {
                img.setPixelColor(x, y, background);
            }
        }
    }
    return img;
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

    if (const auto Pagination = obj.value("Pagination").toObject(); !Pagination.isEmpty()) {
        if (const auto Distance = Pagination.value("Distance").toObject(); !Distance.isEmpty()) {
            m_pagination.DTSideDistance     = Distance.value("SideDistance").toInt();
            m_pagination.DTBottomDistance   = Distance.value("BottomDistance").toInt();
            m_pagination.DTIntervalDistance = Distance.value("IntervalDistance").toInt();
        }
        if (const auto Line = Pagination.value("Line").toObject(); !Line.isEmpty()) {
            m_pagination.Line = std::pair(Line.value("Width").toInt(), Line.value("Height").toInt());
        }
        if (const auto Text = Pagination.value("Text").toObject(); !Text.isEmpty()) {
            m_pagination.Text = std::pair(Text.value("FontSize").toInt(), Text.value("Height").toInt());
        }
        if (const auto Number = Pagination.value("Number").toObject(); !Number.isEmpty()) {
            m_pagination.Number = std::pair(Number.value("FontSize").toInt(), Number.value("Height").toInt());
        }
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
            font.setFamily(FONT_HAN_SANS);
            m_scenePainter->setFont(font);
            m_scenePainter->drawText(xpos, ypos, Headline);

            QFontMetrics fm(font);
            auto w = fm.horizontalAdvance(Headline);
            w += xspace;
            //TODO magic code for font size
            font.setPixelSize(60);
            font.setFamily(FONT_YUANTI);
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
            font.setFamily(FONT_YUANTI);
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
                        font.setFamily(FONT_YAHEI);
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
        const int space         = 20;
        const int xpos          = 520;
        const int AgeInt        = Element.value("Age").toString().toInt();
        const QString name      = QString("我叫%1").arg(Element.value("Name").toString());
        const QString AgeStr    = QString("%1岁%2个月啦").arg(AgeInt/12).arg(AgeInt%12);
        auto font         = m_scenePainter->font();
        //TODO font size
        font.setPixelSize(88);
        font.setFamily(FONT_HAN_SANS);
        m_scenePainter->setFont(font);
        m_scenePainter->setPen(Qt::GlobalColor::white);
        m_scenePainter->drawText(1050, 1000, name);
        QFontMetrics fm(font);
        m_scenePainter->drawText(1050, 1000 + fm.height(), AgeStr);

        font.setPixelSize(60);
        font.setFamily(FONT_YUANTI);
        m_scenePainter->setFont(font);
        m_scenePainter->setPen(QColor("#46e6b3"));
        m_scenePainter->drawText(xpos, 1450, "我的幼儿园");

        font.setPixelSize(48);
        fm = QFontMetrics(font);

        m_scenePainter->setFont(font);
        m_scenePainter->setPen(Qt::GlobalColor::black);

        auto ypos = 1450 + fm.height() + space;
        m_scenePainter->drawText(xpos,
                                 ypos,
                                 Element.value("KindergartenName").toString());

        ypos += fm.height() + space;
        m_scenePainter->drawText(xpos,
                                 ypos,
                                 Element.value("ClazzName").toString());

        ypos = 2035;
        font.setPixelSize(60);
        m_scenePainter->setFont(font);
        m_scenePainter->setPen(QColor("#46e6b3"));
        m_scenePainter->drawText(xpos, ypos, "我的老师");

        font.setPixelSize(48);
        fm = QFontMetrics(font);

        m_scenePainter->setFont(font);
        m_scenePainter->setPen(Qt::GlobalColor::black);

        ypos += fm.height() + space;
        m_scenePainter->drawText(xpos,
                                 ypos,
                                 Element.value("Teachers").toString());


        ypos = 2710;
        font.setPixelSize(60);
        m_scenePainter->setFont(font);
        m_scenePainter->setPen(QColor("#46e6b3"));
        m_scenePainter->drawText(xpos, ypos, "我最喜欢");

        font.setPixelSize(48);
        fm = QFontMetrics(font);

        m_scenePainter->setFont(font);
        m_scenePainter->setPen(Qt::GlobalColor::black);

        ypos += fm.height() + space;
        m_scenePainter->drawText(xpos,
                                 ypos,
                                 Element.value("Hobbies").toString());
    }
}

void PreviewWidget::drawGraduationPhotoPage(const QJsonObject &node)
{
    //title color #8c6b5b , sub #8d715f
    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        const QString title = QString("%1%2").arg(Element.value("ClazzName").toString())
                                  .arg(Element.value("Title").toString().replace("#", ""));
        const QString subTitle("我和小伙伴们一起长大");

        //TODO magic code
        const int space = 300;
        const int border = 20;
        int xpos = 300;
        int ypos = 3200;
        // int xpos = 1000;
        // int ypos = 1000;

        QFont font = m_scenePainter->font();
        //TODO magic size for font
        font.setPixelSize(88);
        font.setFamily(FONT_HAN_SANS);
        QFontMetrics fm(font);

        const int wDelta = m_pageSize.PageWidth - xpos - fm.height();

        m_scenePainter->setFont(font);
        m_scenePainter->setPen(QColor("#8c6b5b"));
          // m_scenePainter->drawText(xpos, ypos - fm.descent(), title);

        m_scenePainter->translate(xpos, ypos);
        m_scenePainter->rotate(-90);
        m_scenePainter->drawText(0, - fm.descent(), title);

        font.setPixelSize(48);
        font.setFamily(FONT_YUANTI);
        m_scenePainter->setFont(font);
        m_scenePainter->setPen(QColor("#8d715f"));
        m_scenePainter->drawText(fm.horizontalAdvance(title) + space, - fm.descent(), subTitle);

        m_scenePainter->rotate(90);
        m_scenePainter->translate(-xpos , -ypos);

        if (const auto Image = Element.value("Image").toObject(); !Image.isEmpty()) {
            if (QImage img; img.load(GET_FILE(Image.value("URL").toString()))) {

#if 0
                // const int w = Image.value("Width").toInt();
                // const int h = Image.value("Height").toInt();

                if (img.width() > img.height()) {
                    img = img.scaled(m_pageSize.FeedPageHeight, m_pageSize.FeedPageWidth,
                               Qt::AspectRatioMode::KeepAspectRatio,
                               Qt::TransformationMode::SmoothTransformation);
                } else {
                    img = img.scaled(m_pageSize.FeedPageWidth, m_pageSize.FeedPageHeight,
                                     Qt::AspectRatioMode::KeepAspectRatio,
                                     Qt::TransformationMode::SmoothTransformation);
                }
                const int w = img.width();
                const int h = img.height();

                xpos = (m_pageSize.PageWidth - wDelta) + (wDelta - qMin(w, h))/2;
                // ypos = qMax(w, h) + (m_pageSize.PageHeight - qMax(w, h))/2;

                if (w > h) { // rotate -90
                    ypos = qMax(w, h) + (m_pageSize.PageHeight - qMax(w, h))/2;
                    m_scenePainter->translate(xpos, ypos);
                    m_scenePainter->rotate(-90);
                    m_scenePainter->drawImage(0, 0, img);

                    m_scenePainter->rotate(90);
                    m_scenePainter->translate(-xpos , -ypos);
                } else {
                    ypos = (m_pageSize.PageHeight - qMax(w, h))/2;
                    m_scenePainter->drawImage(xpos, ypos, img);
                }
#else
                const int Width = qMin(wDelta - border*6, (int)Image.value("Width").toDouble());
                const int Height = Image.value("Height").toDouble();
                const int xc = Image.value("XCoordinate").toDouble();
                const int yc = Image.value("YCoordinate").toDouble();
                const QColor bgColor("#fddabc");

                if (img.width() > img.height()) { // rotate -90
                    img = img.scaled(Height, Width,
                                     Qt::AspectRatioMode::KeepAspectRatio,
                                     Qt::TransformationMode::SmoothTransformation);
                    img = img.scaledToHeight(Width, Qt::SmoothTransformation);
                    if (img.width() > m_pageSize.FeedPageHeight) {
                        img = img.scaledToWidth(m_pageSize.FeedPageHeight);
                    }
                } else {
                    img = img.scaledToWidth(Width, Qt::SmoothTransformation);
                }
                const int pmW = img.width() + border * 2;//qMin(img.width(), Width) + border *2;
                const int pmH = img.height() + border *2;//qMin(img.height(), Height) + border *2;

                QPixmap pm(pmW, pmH);
                pm.fill(Qt::GlobalColor::transparent);

                QPainter p(&pm);
                p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

                p.setPen(bgColor);
                p.setBrush(bgColor);
                p.drawRoundedRect(0, 0, pm.width(), pm.height(), 20, 20);

                QPainterPath path;
                // path.addRoundedRect(0, 0, pm.width(), pm.height(), 20, 20);
                path.addRoundedRect(border, border, img.width(), img.height(), 20, 20);
                p.setClipPath(path);
                p.drawImage(QPoint(border, border), img);

                xpos = (m_pageSize.PageWidth - wDelta) + (wDelta - qMin(pm.width(), pm.height()))/2;
                // ypos = qMax(w, h) + (m_pageSize.PageHeight - qMax(w, h))/2;

                if (pm.width() > pm.height()) { // rotate -90
                    ypos = qMax(pm.width(), pm.height())
                           + (m_pageSize.PageHeight - qMax(pm.width(), pm.height()))/2;
                    m_scenePainter->translate(xpos, ypos);
                    m_scenePainter->rotate(-90);
                    m_scenePainter->drawPixmap(0, 0, pm);

                    m_scenePainter->rotate(90);
                    m_scenePainter->translate(-xpos , -ypos);
                } else {
                    ypos = (m_pageSize.PageHeight - qMax(pm.width(), pm.height()))/2;
                    m_scenePainter->drawPixmap(xpos, ypos, pm);
                }
#endif
            }
        }
    }
    // m_scenePainter->drawRect(0,0, 500, 500);

}

void PreviewWidget::drawGraduationMoviePaget(const QJsonObject &node)
{
    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        if (const auto Image = Element.value("Image").toObject(); !Image.empty()) {
            if (QImage img; img.load(GET_FILE(Image.value("URL").toString()))) {
                //based on background image size
                int xpos = 500;
                int ypos = 790;
                const QSize bgRect(1460, 1100);
                img = img.scaledToWidth(bgRect.width() *95/100, Qt::SmoothTransformation);
                if (img.height() > bgRect.height()) {
                    img = img.scaledToHeight(bgRect.height() *95/100, Qt::SmoothTransformation);
                }
                xpos += (bgRect.width() - img.width())/2;
                ypos += (bgRect.height() - img.height())/2;
                m_scenePainter->drawImage(xpos, ypos, img);
            }
        }
        if (const auto OrginURL = Element.value("OrginURL").toString(); !OrginURL.isEmpty()) {
            const int ypos  = 2000;
            const int qrs   = 400;
            const int xpos  = (m_pageSize.PageWidth - qrs)/2;
            const auto uri  = QCryptographicHash::hash(OrginURL.toUtf8(), QCryptographicHash::Md5).toHex();
            const auto text = QString("%1/%2.%3?inline=true").arg(BARCODE_MEDIA_URI).arg(uri).arg(dotExtension(OrginURL));

            auto img = generateBarcode(text, qrs, qrs);
            m_scenePainter->drawImage(xpos, ypos, img);
        }
    }
}

void PreviewWidget::drawGraduationDreamPage(const QJsonObject &node)
{
    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        if (const auto Images = Element.value("Images").toArray(); !Images.empty()) {
            //TODO only draw first image atm
            if (const auto Image = Images.at(0).toObject(); !Image.isEmpty()) {
                if (QImage img; img.load(GET_FILE(Images.at(0).toObject().value("URL").toString()))) {
                    img = img.scaled(m_pageSize.PageWidth *3/5,
                                     m_pageSize.PageHeight *3/5,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);

                    const int xpos = (m_pageSize.PageWidth - img.width())/2;
                    const int ypos = (m_pageSize.PageHeight - img.height())/2;

                    m_scenePainter->setBrush(Qt::GlobalColor::white);
                    m_scenePainter->setPen(Qt::GlobalColor::white);
                    m_scenePainter->drawRoundedRect(xpos - 10, ypos - 10,
                                                    img.width() + 20, img.height() + 20, 20, 20);

                    m_scenePainter->setBrush(Qt::GlobalColor::black);
                    m_scenePainter->setPen(Qt::GlobalColor::black);

                    QPixmap pm(img.width(), img.height());
                    pm.fill(Qt::GlobalColor::transparent);

                    QPainter p(&pm);
                    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

                    QPainterPath path;
                    path.addRoundedRect(0, 0, pm.width(), pm.height(), 20, 20);
                    p.setClipPath(path);
                    p.drawImage(pm.rect(), img);
                    m_scenePainter->drawPixmap(xpos, ypos, pm);
                }
            }
        }
    }
}

void PreviewWidget::drawHybridSubject(const QJsonObject &node, const QJsonObject &property)
{
    auto Elements = node.value("Elements").toArray();
    if (Elements.isEmpty()) {
        return;
    }
    //TODO DividingLines
    auto DividingLines = node.value("DividingLines").toArray();

    for (const auto &it : Elements) {
        const auto object = it.toObject();
        if (object.isEmpty()) {
            continue;
        }
        const auto Label = object.value("Label").toObject();
        if (Label.isEmpty()) {
            continue;
        }
        const auto Body = object.value("Body").toObject();
        const auto Head = object.value("Head").toObject();
        //TODO draw lines
        const bool IsRenderDividingLine = object.value("IsRenderDividingLine").toBool();

        const auto FeedType = object.value("FeedType").toString();
        qDebug()<<Q_FUNC_INFO<<"FeedType "<<FeedType;

#if 0
        if (FeedType == QLatin1StringView("GuardianCollectionFeed")
            || FeedType == QLatin1StringView("TeacherCollectionFeed")
            /*|| FeedType == QLatin1StringView("GuardianTaskFeed")*/) {
#else
        if (const auto Type = property.value("Type").toString();
            Type == QLatin1StringView("hybrid-subject")
            && (FeedType == QLatin1StringView("GuardianCollectionFeed")
                || FeedType == QLatin1StringView("TeacherCollectionFeed")
                || FeedType == QLatin1StringView("GuardianTaskFeed")) ) {
#endif
            const int space         = 80;
            const int XCoordinate   = Label.value("XCoordinate").toDouble();
            const int YCoordinate   = Label.value("YCoordinate").toDouble();
            const int Width         = m_pageSize.PageWidth - XCoordinate*2 + space;
            const int Height        = object.value("Height").toDouble() + space*2;

            qDebug()<<Q_FUNC_INFO<<"[GuardianCollectionFeed] Height "<<Height
                     <<", Width "<<Width<<", XCoordinate "<<XCoordinate<<", YCoordinate "<<YCoordinate;

            m_scenePainter->setPen(Qt::GlobalColor::white);
            m_scenePainter->setBrush(Qt::GlobalColor::white);
            m_scenePainter->drawRoundedRect(XCoordinate - space/2,
                                            YCoordinate - space,
                                            Width,
                                            Height,
                                            20, 20);
        }
        // if (FeedType == QLatin1StringView("GuardianTaskFeed")) {
        //     //TODO  do nothing atm
        // }

        /** subject **/
        if (const auto Type = Label.value("Type").toString() == QLatin1StringView("subject")) {
            int xpos = Label.value("XCoordinate").toDouble();
            int ypos = Label.value("YCoordinate").toDouble();

            auto font = m_scenePainter->font();
            //TODO font size,
            font.setPixelSize(112);
            font.setFamily(FONT_HAN_SANS);
            m_scenePainter->setFont(font);

            QFontMetrics fm(font);
            //NOTE Background always !empty in this json file,so ignore null check
            if (const auto Color = property.value("Background").toObject().value("Color").toString();
                !Color.isEmpty()) {
                QColor c(Color);
                c.setAlphaF(0.8);
                m_scenePainter->setPen(c);
                m_scenePainter->setBrush(c);
            }
            if (const auto Title = Head.value("Title").toObject(); !Title.isEmpty()) {
                if (const auto Lines = Title.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text         = lo.value("Text").toString();
                        const auto XCoordinates = lo.value("XCoordinates").toArray();
                        const int YCoordinate   = lo.value("YCoordinate").toInt();
#if 0
                        if (Text.size() != XCoordinates.size()) {
                            qWarning()<<Q_FUNC_INFO<<"Text.size() != XCoordinates.size(), ignore XCoordinates";
                            m_scenePainter->drawText(xpos, ypos, Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                xpos += XCoordinates.at(i).toInt();
                                m_scenePainter->drawText(xpos, ypos - fm.descent(), Text.at(i));
                            }
                        }
#else
                        m_scenePainter->drawText(xpos, ypos, QString("‘““  %1").arg(Text));
#endif
                    }
                }
            }

            if (const auto Content = Body.value("Content").toObject(); !Content.isEmpty()) {
                auto font = m_scenePainter->font();
                //TODO font size
                font.setPixelSize(48);
                font.setFamily(FONT_HAN_SANS);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);

                if (const auto Lines = Content.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text         = lo.value("Text").toString();
                        const auto XCoordinates = lo.value("XCoordinates").toArray();
                        const int YCoordinate   = lo.value("YCoordinate").toDouble();

                        if (Text.isEmpty()) {
                            continue;
                        }

                        qDebug()<<Q_FUNC_INFO<<"YCoordinate "<<YCoordinate
                                 <<", Text "<<Text;

                        if (Text.size() != XCoordinates.size()) {
                            qWarning()<<Q_FUNC_INFO<<"[Content] Text.size() != XCoordinates.size(), ignore XCoordinates for text "<<Text;
                            m_scenePainter->drawText(xpos + XCoordinates.at(0).toDouble(),
                                                     ypos + YCoordinate + fm.ascent(),
                                                     Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                m_scenePainter->drawText(xpos + XCoordinates.at(i).toDouble(),
                                                         ypos + YCoordinate + fm.ascent(),
                                                         Text.at(i));
                            }
                        }
                    }
                }
            }




        } /** end subject **/

        if (const auto Type = Label.value("Type").toString() == QLatin1StringView("feed")) {
            const int xpos = Label.value("XCoordinate").toDouble();
            const int ypos = Label.value("YCoordinate").toDouble();

            const auto Content = Label.value("Content").toString();

            if (!Content.isEmpty()) {
                //NOTE Background always !empty in this json file,so ignore null check
                if (const auto Color = property.value("Background").toObject().value("Color").toString();
                    !Color.isEmpty()) {
                    m_scenePainter->setPen(QColor(Color));
                    m_scenePainter->setBrush(QColor(Color));
                }
                m_scenePainter->drawRoundedRect(xpos, ypos,
                                                Label.value("Width").toDouble(),
                                                Label.value("Height").toDouble(),
                                                10, 10);
            }

            m_scenePainter->translate(xpos, ypos);

            // {
            //     m_scenePainter->setPen(Qt::GlobalColor::magenta);
            //     m_scenePainter->setBrush(Qt::GlobalColor::transparent);
            //     m_scenePainter->drawRect(0, 0, 500, 500);
            // }

            //draw date from label tag
            if (auto cr = Content.split("-"); cr.size() == 3) {
                m_scenePainter->setPen(Qt::GlobalColor::white);
                m_scenePainter->setBrush(Qt::GlobalColor::white);
                auto font = m_scenePainter->font();
                font.setPixelSize(88);
                font.setFamily(FONT_HAN_SANS);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);
                int w = fm.horizontalAdvance(cr.at(2));
                int x = (Label.value("Width").toInt() - w)/2;
                int y = fm.ascent();

                m_scenePainter->drawText(x, y, cr.takeLast());

                y = fm.height();

                font.setPixelSize(48);
                font.setFamily(FONT_YUANTI);
                fm = QFontMetrics(font);
                m_scenePainter->setFont(font);

                const QString text = cr.join("/");
                w = fm.horizontalAdvance(text);
                x = (Label.value("Width").toDouble() - w)/2;
                y += fm.ascent();

                m_scenePainter->drawText(x, y, text);
            }

            m_scenePainter->setPen(Qt::GlobalColor::black);
            m_scenePainter->setBrush(Qt::GlobalColor::black);

            if (const auto Title = Head.value("Title").toObject(); !Title.isEmpty()) {
                auto font = m_scenePainter->font();
                font.setPixelSize(88);
                font.setFamily(FONT_HAN_SANS);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);

                if (const auto Lines = Title.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text         = lo.value("Text").toString();
                        const auto XCoordinates = lo.value("XCoordinates").toArray();
                        const int YCoordinate   = lo.value("YCoordinate").toDouble();
                        // m_scenePainter->drawText(xpos, ypos, Text);

                        if (Text.size() != XCoordinates.size()) {
                            qWarning()<<Q_FUNC_INFO<<"Text.size() != XCoordinates.size(), ignore XCoordinates";
                            m_scenePainter->drawText(XCoordinates.at(0).toDouble(), YCoordinate + fm.ascent(), Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                // xpos += XCoordinates.at(i).toInt();
                                m_scenePainter->drawText(XCoordinates.at(i).toDouble(),
                                                         YCoordinate + fm.ascent(),
                                                         Text.at(i));
                            }
                        }
                    }
                }
            }
            if (const auto Icon = Head.value("Icon").toObject(); !Icon.isEmpty()) {
                auto font = m_scenePainter->font();
                font.setPixelSize(Icon.value("Height").toDouble());
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);
                m_scenePainter->drawText(Icon.value("XCoordinate").toDouble(),
                                         Icon.value("YCoordinate").toDouble() + fm.ascent(),
                                         "👩‍🏫");
            }
            if (const auto Mark = Head.value("Mark").toObject(); !Mark.isEmpty()) {
                auto font = m_scenePainter->font();
                //TODO font size
                font.setPixelSize(48);
                font.setFamily(FONT_YUANTI);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);

                if (const auto Lines = Mark.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text         = lo.value("Text").toString();
                        const auto XCoordinates = lo.value("XCoordinates").toArray();
                        const int YCoordinate   = lo.value("YCoordinate").toDouble();
                        if (Text.size() != XCoordinates.size()) {
                            qWarning()<<Q_FUNC_INFO<<"[Mark] Text.size() != XCoordinates.size(), ignore XCoordinates";
                            m_scenePainter->drawText(XCoordinates.at(0).toDouble(), YCoordinate + fm.height() + fm.descent(), Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                m_scenePainter->drawText(XCoordinates.at(i).toDouble(),
                                                         YCoordinate + fm.height() + fm.descent(),
                                                         Text.at(i));
                            }
                        }
                    }
                }
            }
            if (const auto Tag = Head.value("Tag").toObject(); !Tag.isEmpty()) {
                if (const auto TagIcon = Tag.value("TagIcon").toObject(); !TagIcon.isEmpty()) {
                    auto font = m_scenePainter->font();
                    font.setPixelSize(TagIcon.value("Height").toDouble());
                    m_scenePainter->setFont(font);
                    QFontMetrics fm(font);

                    const int TagType = TagIcon.value("TagType").toInt();
                    if (TagType == 1) {
                        m_scenePainter->drawText(TagIcon.value("XCoordinate").toDouble(),
                                                 TagIcon.value("YCoordinate").toDouble() + fm.height(),
                                                 "♥️");
                    }
                }
                if (const auto TagText = Tag.value("TagText").toObject(); !TagText.isEmpty()) {
                    if (const auto Lines = TagText.value("Lines").toArray(); !Lines.isEmpty()) {
                        //TODO font size
                        auto font = m_scenePainter->font();
                        font.setPixelSize(48);
                        font.setFamily(FONT_YUANTI);
                        m_scenePainter->setFont(font);

                        QFontMetrics fm(font);

                        for (const auto &l : Lines) {
                            auto lo = l.toObject();
                            if (lo.isEmpty()) {
                                continue;
                            }
                            const auto Text         = lo.value("Text").toString();
                            const auto XCoordinates = lo.value("XCoordinates").toArray();
                            const int YCoordinate   = lo.value("YCoordinate").toDouble();
                            // m_scenePainter->drawText(xpos, ypos, Text);

                            if (Text.size() != XCoordinates.size()) {
                                qWarning()<<Q_FUNC_INFO<<"Text.size() != XCoordinates.size(), ignore XCoordinates";
                                m_scenePainter->drawText(XCoordinates.at(0).toDouble(),
                                                         YCoordinate + fm.height() + fm.descent(),
                                                         Text);
                            } else {
                                for (int i=0; i<Text.size(); ++i) {
                                    // xpos += XCoordinates.at(i).toInt();
                                    m_scenePainter->drawText(XCoordinates.at(i).toDouble(),
                                                             YCoordinate + fm.height() + fm.descent(),
                                                             Text.at(i));
                                }
                            }
                        }
                    }
                }
            }
            if (const auto Content = Body.value("Content").toObject(); !Content.isEmpty()) {
                auto font = m_scenePainter->font();
                //TODO font size
                font.setPixelSize(48);
                font.setFamily(FONT_YUANTI);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);

                if (const auto Lines = Content.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text         = lo.value("Text").toString();
                        const auto XCoordinates = lo.value("XCoordinates").toArray();
                        const int YCoordinate   = lo.value("YCoordinate").toDouble();

                        if (Text.isEmpty()) {
                            continue;
                        }

                        qDebug()<<Q_FUNC_INFO<<"YCoordinate "<<YCoordinate
                                 <<", Text "<<Text;

                        if (Text.size() != XCoordinates.size()) {
                            qWarning()<<Q_FUNC_INFO<<"[Content] Text.size() != XCoordinates.size(), ignore XCoordinates for text "<<Text;
                            m_scenePainter->drawText(XCoordinates.at(0).toDouble(), YCoordinate + fm.ascent(), Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                m_scenePainter->drawText(XCoordinates.at(i).toDouble(),
                                                         YCoordinate + fm.ascent(),
                                                         Text.at(i));
                            }
                        }
                    }
                }
            }
            if (const auto Media = Body.value("Media").toObject(); !Media.isEmpty()) {
                if (const auto Elements = Media.value("Elements").toArray(); !Elements.isEmpty()) {
                    for (const auto &e : Elements) {
                        if (const auto obj = e.toObject(); !obj.empty()) {

                            // qDebug()<<Q_FUNC_INFO<<"media object "<<e
                            //          <<", file "<<GET_FILE(obj.value("URL").toString());

                            const auto Type         = obj.value("Type").toString();
                            const int Width         = obj.value("Width").toDouble();
                            const int Height        = obj.value("Height").toDouble();
                            const int XCoordinate   = obj.value("XCoordinate").toDouble();
                            const int YCoordinate   = obj.value("YCoordinate").toDouble();
                            const int Rotation      = obj.value("Rotation").toDouble();

                            // qDebug()<<Q_FUNC_INFO<<"file image, Height "<<Height
                            //          <<", Width "<<Width<<", XCoordinate "<<XCoordinate<<", YCoordinate "<<YCoordinate
                            //          <<", Rotation "<<Rotation;

                            if (Type == QLatin1StringView("image")) {
                                if (QImage img; img.load(GET_FILE(obj.value("URL").toString()))) {
                                    if (qAbs(Rotation) != 0) {
                                        img = img.scaledToHeight(Width, Qt::SmoothTransformation);

                                        QPixmap pm(qMax(img.height(), img.width()),
                                                   qMax(img.height(), img.width()));
                                        pm.fill(Qt::GlobalColor::transparent);

                                        QPainter p(&pm);
                                        p.translate(pm.width()/2, pm.height()/2);
                                        p.rotate(Rotation);
                                        p.drawImage(-pm.height()/2, -pm.width()/2, img);
                                        m_scenePainter->drawPixmap(XCoordinate, YCoordinate, pm);
                                    }
                                    else {
                                        img = img.scaled(Width, Height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                                        if (img.width() > Width) {
                                            img = img.scaledToWidth(Width, Qt::SmoothTransformation);
                                        }
                                        else if (img.height() > Height) {
                                            img = img.scaledToHeight(Height, Qt::SmoothTransformation);
                                        }
                                        m_scenePainter->drawImage(XCoordinate, YCoordinate, img);
                                    }
                                }
                            }
                            else if (Type == QLatin1StringView("colorblock")) {
                                //NOTE Background always !empty in this json file,so ignore null check
                                if (const auto Color = property.value("Background").toObject().value("Color").toString();
                                    !Color.isEmpty()) {
                                    QColor c(Color);
                                    c.setAlphaF(0.5);
                                    m_scenePainter->setPen(c);
                                    m_scenePainter->setBrush(c);
                                    m_scenePainter->drawRect(XCoordinate, YCoordinate, Width, Height);
                                }
                            }
                        }
                    }
                }
            }
            if (const auto Video = Body.value("Video").toObject(); !Video.isEmpty()) {
                const int Width         = Video.value("Width").toDouble();
                const int Height        = Video.value("Height").toDouble();
                const int XCoordinate   = Video.value("XCoordinate").toDouble();
                const int YCoordinate   = Video.value("YCoordinate").toDouble();
                if (const auto Image = Video.value("Image").toObject(); !Image.isEmpty()) {
                    if (QImage img; img.load(GET_FILE(Image.value("URL").toString()))) {
                        const int w = qMin(Width, (int)Image.value("Width").toDouble());
                        const int h = qMin(Height, (int)Image.value("Height").toDouble());
                        const int xc = Image.value("XCoordinate").toDouble();
                        const int yc = Image.value("YCoordinate").toDouble();
                        img = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        if (img.width() > w) {
                            img = img.scaledToWidth(Width, Qt::SmoothTransformation);
                        }
                        else if (img.height() > h) {
                            img = img.scaledToHeight(Height, Qt::SmoothTransformation);
                        }
                        m_scenePainter->drawImage(XCoordinate + xc, YCoordinate + yc, img);
                    }
                }
                //TODO QR code
            }
            if (const auto Template = Body.value("Template").toObject(); !Template.isEmpty()) {

                const int Width         = Template.value("Width").toDouble();
                const int Height        = Template.value("Height").toDouble();
                const int XCoordinate   = Template.value("XCoordinate").toDouble();
                const int YCoordinate   = Template.value("YCoordinate").toDouble();
                const auto Type         = Template.value("Type").toString();
                const auto SubType      = Template.value("SubType").toString();

                if (Type == QLatin1StringView("VV")) {
                    // m_scenePainter->setPen(Qt::GlobalColor::green);
                    // m_scenePainter->setBrush(Qt::GlobalColor::green);
                    // m_scenePainter->drawRect(XCoordinate, YCoordinate, Width, Height);
                    //TODO add white backgroud for images

                    int rotation = 5;
                    int yoffset = 0;

                    if (const auto Images = Template.value("Images").toArray(); !Images.isEmpty()) {

                        for (const auto &it : Images) {
                            if (const auto image = it.toObject(); !image.isEmpty()) {
                                if (QImage img; img.load(GET_FILE(image.value("URL").toString()))) {
                                    rotation = -rotation;
                                    const int w = qMin(Width, (int)image.value("Width").toDouble());
                                    const int h = qMin(Height, (int)image.value("Height").toDouble());
                                    const int xc = image.value("XCoordinate").toDouble();
                                    const int yc = image.value("YCoordinate").toDouble();
                                    img = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                                    if (img.width() > w) {
                                        img = img.scaledToWidth(Width, Qt::SmoothTransformation);
                                    }
                                    else if (img.height() > h) {
                                        img = img.scaledToHeight(Height, Qt::SmoothTransformation);
                                    }
#if 0
                                    // m_scenePainter->translate(img.width()/2, img.height()/2);
                                    // m_scenePainter->rotate(rotation);
                                    // m_scenePainter->translate(-img.width()/2, -img.height()/2);
                                    // m_scenePainter->drawImage(xc, yc + yoffset, img);

                                    // //reset painter
                                    // m_scenePainter->translate(img.width()/2, img.height()/2);
                                    // m_scenePainter->rotate(-rotation);
                                    // m_scenePainter->translate(-img.width()/2, -img.height()/2);
#else
                                    const int border = 20;
                                    QPixmap pm(img.width() + border*2, img.height() + border*2);
                                    pm.fill(Qt::GlobalColor::transparent);

                                    QPainter p(&pm);
                                    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

                                    p.setPen(QColor("#f3f3f3"));
                                    p.setBrush(QColor("#f3f3f3"));
                                    p.drawRoundedRect(0, 0, pm.width(), pm.height(), 20, 20);

                                    QPainterPath path;
                                    // path.addRoundedRect(0, 0, pm.width(), pm.height(), 20, 20);
                                    path.addRoundedRect(border, border, img.width(), img.height(), 20, 20);
                                    p.setClipPath(path);
                                    p.drawImage(QPoint(border, border), img);

                                    m_scenePainter->translate(pm.width()/2, pm.height()/2);
                                    m_scenePainter->rotate(rotation);
                                    m_scenePainter->translate(-pm.width()/2, -pm.height()/2);
                                    m_scenePainter->drawPixmap(xc, yc + yoffset, pm);

                                    //reset painter
                                    m_scenePainter->translate(pm.width()/2, pm.height()/2);
                                    m_scenePainter->rotate(-rotation);
                                    m_scenePainter->translate(-pm.width()/2, -pm.height()/2);
#endif
                                    yoffset += img.height() *3/5;

                                }
                            }
                        }
                    }
                }
            }



            m_scenePainter->translate(-xpos,  -ypos);
        } //end feed
    }
}

void PreviewWidget::drawFeedPage(const QJsonObject &node, const QJsonObject &property)
{
    drawHybridSubject(node, property);
}

void PreviewWidget::drawPhysicalExaminationPage(const QJsonObject &node, const QJsonObject &property)
{
    const int xc = 700;
    const int yc = 1000;
    const QColor lineColor("#ff9c2b");
    if (const auto Elements = node.value("Elements").toArray(); !Elements.isEmpty()) {
        //NOTE only draw first node here
        if (const auto obj = Elements.first().toObject(); !obj.isEmpty()) {
            const auto Date     = obj.value("Date").toString();
            const auto Headline = obj.value("Headline").toString();

            const int lineW = 40;
            const int lineH = 150;

            int xpos = xc;
            int ypos = yc;

            m_scenePainter->setPen(lineColor);
            m_scenePainter->setBrush(lineColor);
            m_scenePainter->drawRect(xpos, ypos, lineW, lineH);

            m_scenePainter->setPen(Qt::GlobalColor::black);
            m_scenePainter->setBrush(Qt::GlobalColor::black);

            auto font = m_scenePainter->font();
            font.setPixelSize(72);
            font.setFamily(FONT_YAHEI);
            m_scenePainter->setFont(font);

            QFontMetrics fm(font);

            xpos += lineW + 30;

            m_scenePainter->drawText(xpos, ypos + fm.ascent(), Headline);

            ypos += fm.height() + 40;

            font.setPixelSize(48);
            font.setFamily(FONT_YUANTI);
            m_scenePainter->setFont(font);
            m_scenePainter->drawText(xpos, ypos, Date);

            ypos += 100;

            //📏,  ⚖, 👀,🩸,🦷
            if (const auto Data = obj.value("Data").toObject(); !Date.isEmpty()) {
                const int spcae = 120;
                if (const auto height = Data.value("height").toObject(); !obj.empty()) {
                    const QString Value = QString::number(height.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "📏");
                    m_scenePainter->drawText(xc + 100, ypos, height.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                                             ypos,
                                             height.value("Unit").toString());
                    if (const auto Assessement = height.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, height.value("Assessement").toString());
                    }
                }

                ypos += spcae;
                if (const auto weight = Data.value("weight").toObject(); !obj.empty()) {
                    const QString Value = QString::number(weight.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "⚖");
                    m_scenePainter->drawText(xc + 100, ypos, weight.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                                             ypos,
                                             weight.value("Unit").toString());

                    if (const auto Assessement = weight.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, weight.value("Assessement").toString());
                    }
                }

                ypos += spcae;
                if (const auto leftEye = Data.value("leftEye").toObject(); !obj.empty()) {
                    const QString Value = QString::number(leftEye.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "👀");
                    m_scenePainter->drawText(xc + 100, ypos, leftEye.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    // m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                    //                          ypos,
                    //                          leftEye.value("Unit").toString());

                    if (const auto Assessement = leftEye.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, leftEye.value("Assessement").toString());
                    }
                }

                ypos += spcae;
                if (const auto rightEye = Data.value("rightEye").toObject(); !obj.empty()) {
                    const QString Value = QString::number(rightEye.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "👀");
                    m_scenePainter->drawText(xc + 100, ypos, rightEye.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    // m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                    //                          ypos,
                    //                          rightEye.value("Unit").toString());

                    if (const auto Assessement = rightEye.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, rightEye.value("Assessement").toString());
                    }
                }

                ypos += spcae;
                if (const auto heme = Data.value("heme").toObject(); !obj.empty()) {
                    const QString Value = QString::number(heme.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "🩸");
                    m_scenePainter->drawText(xc + 100, ypos, heme.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                                             ypos,
                                             heme.value("Unit").toString());

                    if (const auto Assessement = heme.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, heme.value("Assessement").toString());
                    }
                }

                ypos += spcae;
                if (const auto caries = Data.value("caries").toObject(); !obj.empty()) {
                    const QString Value = QString::number(caries.value("Value").toDouble());
                    m_scenePainter->drawText(xc, ypos, "🦷");
                    m_scenePainter->drawText(xc + 100, ypos, caries.value("Name").toString());
                    m_scenePainter->drawText(xc + 400, ypos, Value);
                    m_scenePainter->drawText(xc + 400 + fm.horizontalAdvance(Value),
                                             ypos,
                                             caries.value("Unit").toString());

                    if (const auto Assessement = caries.value("Assessement").toString(); Assessement.isEmpty()) {
                        m_scenePainter->drawText(xc + 800, ypos, QLatin1StringView("/"));
                    }
                    else {
                        m_scenePainter->drawText(xc + 800, ypos, caries.value("Assessement").toString());
                    }
                }

            }
        }


    }

}

void PreviewWidget::drawEWishPage(const QJsonObject &node, const QJsonObject &property)
{
    if (const auto Elements = node.value("Elements").toArray(); !Elements.isEmpty()) {
        for (const auto &ele : Elements) {
            const auto obj = ele.toObject();
            if (obj.isEmpty()) {
                continue;
            }
            const auto Wish = obj.value("Wish").toObject();
            if (Wish.isEmpty()) {
                continue;
            }
            const auto XCoordinate  = obj.value("XCoordinate").toDouble();
            const auto YCoordinate  = obj.value("YCoordinate").toDouble();
            const auto Width        = Wish.value("Width").toDouble();
            const auto Height       = Wish.value("Height").toDouble();
            const auto bgXC       = XCoordinate + Wish.value("XCoordinate").toDouble();
            const auto bgYC       = YCoordinate + Wish.value("YCoordinate").toDouble();

            //draw bg
            m_scenePainter->setPen(Qt::GlobalColor::white);
            m_scenePainter->setBrush(Qt::GlobalColor::white);
            m_scenePainter->drawRoundedRect(bgXC, bgYC, Width, Height, 20, 20);

            if (const auto Stamp = Wish.value("Stamp").toObject(); !Stamp.isEmpty()) {
                const auto sXC = Stamp.value("XCoordinate").toDouble();
                const auto sYC = Stamp.value("YCoordinate").toDouble();
                const int sW = 300;
                const int sH = 100;

                QColor color("#57b59f");
                m_scenePainter->setPen(color);
                m_scenePainter->setBrush(color);
                m_scenePainter->drawRoundedRect(bgXC + sXC - 50,
                                                bgYC + sYC,
                                                sW, sH,
                                                10, 10);

                m_scenePainter->setPen(Qt::GlobalColor::white);
                m_scenePainter->setBrush(Qt::GlobalColor::white);
                auto font = m_scenePainter->font();
                font.setPixelSize(48);
                font.setFamily(FONT_YUANTI);
                m_scenePainter->setFont(font);

                QFontMetrics fm(font);

                m_scenePainter->drawText(bgXC + sXC,
                                         bgYC + sYC + sH/2 + fm.ascent()/2,
                                         "老师的话");
            }

            m_scenePainter->setPen(Qt::GlobalColor::black);
            m_scenePainter->setBrush(Qt::GlobalColor::black);
            auto font = m_scenePainter->font();
            font.setPixelSize(48);
            font.setFamily(FONT_YUANTI);
            m_scenePainter->setFont(font);

            QFontMetrics fm(font);

            if (const auto Label = Wish.value("Label").toObject(); !Label.isEmpty()) {
                const auto Text = Label.value("Text").toString();
                const auto lxc  = Label.value("XCoordinate").toDouble();
                const auto lyc  = Label.value("YCoordinate").toDouble();

                m_scenePainter->drawText(XCoordinate + lxc,
                                         YCoordinate + lyc + fm.ascent() /*- fm.descent()*/,
                                         Text);
            }
            if (const auto Signature = Wish.value("Signature").toObject(); !Signature.isEmpty()) {
                if (const auto Line = Signature.value("Line").toObject(); !Line.isEmpty()) {
                    const auto Text = Line.value("Text").toString();
                    const auto lyc  = Line.value("YCoordinate").toDouble();
                    const auto lxcs = Line.value("XCoordinates").toArray();

                    if (Text.size() != lxcs.size()) {
                        qWarning()<<Q_FUNC_INFO<<"[Content] Text.size() != XCoordinates.size(), ignore XCoordinates for text "<<Text;
                        m_scenePainter->drawText(XCoordinate + lxcs.at(0).toDouble(),
                                                 YCoordinate + lyc + fm.ascent(), Text);
                    } else {
                        for (int i=0; i<Text.size(); ++i) {
                            m_scenePainter->drawText(XCoordinate + lxcs.at(i).toDouble(),
                                                     YCoordinate + lyc + fm.ascent(),
                                                     Text.at(i));
                        }
                    }
                }
            }
            if (const auto Content = Wish.value("Content").toObject(); !Content.isEmpty()) {
                if (const auto Lines = Content.value("Lines").toArray(); !Lines.isEmpty()) {
                    for (const auto &l : Lines) {
                        const auto lo = l.toObject();
                        if (lo.isEmpty()) {
                            continue;
                        }
                        const auto Text = lo.value("Text").toString();
                        const auto lyc  = lo.value("YCoordinate").toDouble();
                        const auto lxcs = lo.value("XCoordinates").toArray();

                        if (Text.size() != lxcs.size()) {
                            qWarning()<<Q_FUNC_INFO<<"[Content] Text.size() != XCoordinates.size(), ignore XCoordinates for text "<<Text;
                            m_scenePainter->drawText(XCoordinate + lxcs.at(0).toDouble(),
                                                     YCoordinate + lyc + fm.ascent(), Text);
                        } else {
                            for (int i=0; i<Text.size(); ++i) {
                                m_scenePainter->drawText(XCoordinate + lxcs.at(i).toDouble(),
                                                         YCoordinate + lyc + fm.ascent(),
                                                         Text.at(i));
                            }
                        }
                    }
                }
            }
        }
    }
}

void PreviewWidget::drawGraduationAudios(const QJsonObject &node, const QJsonObject &property)
{
    if (const auto Element = node.value("Element").toObject(); !Element.isEmpty()) {
        if (const auto GraduationAudios = Element.value("GraduationAudios").toArray(); !GraduationAudios.isEmpty()) {
            const int cellW = 700;
            const int cellH = 900;
            const int cSpace = 50;
            const int cNum  = 3; //column nums
            const int rNum  = 2; //row nums
            const int avatarS = 280;
            // const int startYpos = 600;
            const QPoint sp((m_pageSize.PageWidth - cellW * cNum - cSpace *(cNum -2))/2,
                            600);
            int xpos = sp.x();
            int ypos = sp.y();

            qDebug()<<Q_FUNC_INFO<<"sp "<<sp;

            for (int i=0; i<GraduationAudios.size(); ++i) {
                const auto obj = GraduationAudios.at(i).toObject();

#define ADD_POS \
    do { \
        if ((i+1) % cNum == 0) { \
                xpos = sp.x(); \
                ypos += cellH + cSpace; \
        } else { \
                xpos += cellW + cSpace; \
        } \
    } while (0);

                if (obj.isEmpty()) {
                    ADD_POS;
                    continue;
                }
                const auto StudentName  = obj.value("StudentName").toString();
                const auto Hobbies      = QString("爱好：%1").arg(obj.value("Hobbies").toString());
                const auto StudentGender= obj.value("StudentGender").toInt(); //2 for girl
                const auto AvatarURL    = obj.value("AvatarURL").toString();

                QPixmap pm(cellW, cellH);
                pm.fill(Qt::GlobalColor::transparent);

                {
                    QPainter p(&pm);
                    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

                    QColor c = Qt::GlobalColor::white;
                    c.setAlphaF(0.3);
                    p.setBrush(c);
                    p.setPen(c);
                    p.drawRoundedRect(0, 0, cellW, cellH, 20, 20);
                }
                {
                    QImage img;
                    if (img.load(GET_FILE(AvatarURL))) {
                        if (img.width() > avatarS) {
                            img = img.scaledToWidth(avatarS, Qt::SmoothTransformation);
                        }
                        if (img.height() > avatarS) {
                            img = img.scaledToHeight(avatarS, Qt::SmoothTransformation);
                        }
                        QPainter p;
                        QPixmap pp(img.width(), img.height());
                        pp.fill(Qt::GlobalColor::transparent);

                        p.begin(&pp);
                        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                        QPainterPath path;
                        path.addEllipse(0, 0,
                                        qMin(pp.width(), pp.height()),
                                        qMin(pp.width(), pp.height()));
                        p.setClipPath(path);
                        p.drawImage(pp.rect(), img);
                        p.end();

                        p.begin(&pm);
                        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                        p.drawPixmap(cSpace, cSpace, pp);
                        p.end();
                    }
                }
                {
                    QPainter p(&pm);
                    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    p.setBrush(Qt::GlobalColor::white);
                    p.setPen(Qt::GlobalColor::white);

                    int x = avatarS + cSpace *2;
                    int y = cSpace;
                    auto font = p.font();
                    font.setPixelSize(48);
                    p.setFont(font);
                    QFontMetrics fm(font);

                    p.drawText(x, y + fm.ascent(), StudentName);

                    y += fm.height() + cSpace;

                    if (StudentGender == 2) { //girl
                        p.setBrush(QColor("#ff79b1"));
                        p.setPen(QColor("#ff79b1"));

                        int w = fm.horizontalAdvance("♀") + 20;
                        int h = fm.height() + 20;
                        p.drawRoundedRect(x - 10,
                                          y - 10,
                                          w, h, 4, 4);

                        p.setBrush(Qt::GlobalColor::white);
                        p.setPen(Qt::GlobalColor::white);
                        p.drawText(x, y + fm.ascent(), "♀");
                    } else {
                        p.setBrush(QColor("#6fc2ff"));
                        p.setPen(QColor("#6fc2ff"));

                        int w = fm.horizontalAdvance("♂") + 20;
                        int h = fm.height() + 20;
                        p.drawRoundedRect(x - 10,
                                          y - 10,
                                          w, h, 4, 4);

                        p.setBrush(Qt::GlobalColor::white);
                        p.setPen(Qt::GlobalColor::white);
                        p.drawText(x, y + fm.ascent(), "♂");
                    }
                }

                {
                    QPainter p(&pm);
                    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    p.setBrush(Qt::GlobalColor::white);
                    p.setPen(Qt::GlobalColor::white);

                    int x = cSpace;
                    int y = avatarS + cSpace *2;
                    auto font = p.font();
                    font.setPixelSize(48);
                    p.setFont(font);
                    // QFontMetrics fm(font);

                    p.drawText(x,
                               y,
                               cellW - cSpace *2,
                               120,
                               Qt::AlignLeft | Qt::TextWordWrap,
                               Hobbies);
                }
                //TODO QR code

                m_scenePainter->drawPixmap(xpos, ypos, pm);
                ADD_POS;
            }
        }
    }
}

void PreviewWidget::drawPagination(const QJsonObject &node)
{
    const int Location      = node.value("Location").toInt();
    const QString Number    = QString("%1").arg(node.value("Number").toInt(), 2, 10, QChar('0'));
    const QString Text      = node.value("Text").toString();
    QFont font              = m_scenePainter->font();
    int ypos                = m_pageSize.PageHeight - m_pagination.DTBottomDistance;

    font.setFamily(FONT_YAHEI);

    m_scenePainter->setPen(Qt::GlobalColor::black);
    m_scenePainter->setBrush(Qt::GlobalColor::black);
    if (Location == 1) { //left
        int xpos = m_pagination.DTSideDistance;
        font.setPixelSize(std::get<0>(m_pagination.Number));
        m_scenePainter->setFont(font);

        QFontMetrics fm(font);
        m_scenePainter->drawText(xpos, ypos - fm.descent(), Number);

        xpos += fm.horizontalAdvance(Number);
        xpos += m_pagination.DTIntervalDistance;

        m_scenePainter->drawRect(xpos,
                                 ypos - std::get<1>(m_pagination.Line),
                                 std::get<0>(m_pagination.Line),
                                 std::get<1>(m_pagination.Line));

        xpos += std::get<0>(m_pagination.Line);
        xpos += m_pagination.DTIntervalDistance;

        font.setPixelSize(std::get<0>(m_pagination.Text));
        fm = QFontMetrics(font);
        m_scenePainter->setFont(font);
        m_scenePainter->drawText(xpos, ypos - fm.descent(), Text);
    }
    else if (Location == 2) { //right
        font.setPixelSize(std::get<0>(m_pagination.Number));
        m_scenePainter->setFont(font);

        QFontMetrics fm(font);
        int xpos = m_pageSize.PageWidth - m_pagination.DTSideDistance;
        xpos    -= fm.horizontalAdvance(Number);
        m_scenePainter->drawText(xpos, ypos - fm.descent(), Number);

        xpos -= m_pagination.DTIntervalDistance;
        xpos -= std::get<0>(m_pagination.Line);

        m_scenePainter->drawRect(xpos,
                                 ypos - std::get<1>(m_pagination.Line),
                                 std::get<0>(m_pagination.Line),
                                 std::get<1>(m_pagination.Line));

        xpos -= m_pagination.DTIntervalDistance;

        font.setPixelSize(std::get<0>(m_pagination.Text));
        m_scenePainter->setFont(font);

        fm      = QFontMetrics(font);
        xpos    -= fm.horizontalAdvance(Text);
        m_scenePainter->drawText(xpos, ypos - fm.descent(), Text);
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
                // img = img.scaledToHeight(Height, Qt::SmoothTransformation);
                img = img.scaledToHeight(m_pageSize.PageHeight, Qt::SmoothTransformation);
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
        font.setFamily(FONT_YAHEI);
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

















