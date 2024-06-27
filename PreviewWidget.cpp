#include "PreviewWidget.h"

#include <QDebug>


PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget{parent}
{

}

PreviewWidget::~PreviewWidget()
{
    qDebug()<<Q_FUNC_INFO<<"----------------";
}
