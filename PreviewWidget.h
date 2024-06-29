#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <QJsonArray>

#include "PropertyData.h"

class PreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget *parent = nullptr);
    virtual ~PreviewWidget();

    bool load(const QString &jsonPath, const QString &mediaPath);

    void drawPage(int pgNum);




    // QWidget interface
protected:
    virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

protected:
    virtual bool parseProperty(const QJsonObject &obj);

private:
    void drawIntroPage(const QJsonObject &node);
    void drawVersionPage(const QJsonObject &node);
    void drawDirectoryPage(const QJsonObject &node);
    void drawProfilePage(const QJsonObject &node);

    void drawBackground(const QJsonObject &PropertyObject);
    void drawElement(const QJsonObject &node);
    void drawTemplateElement(const QJsonObject &node);

private:
    QImage *m_sceneImg = nullptr;
    QPainter *m_scenePainter = nullptr;

    int m_curID = -1;
    QString m_mediaPath;

    PageSize m_pageSize;

    QJsonArray m_pages;
    DividingLine m_dvLine;



};

#endif // PREVIEWWIDGET_H
