#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QMessageBox>

#include "MediaDownloader.h"
#include "PreviewWidget.h"

#define DEV_DBG 1

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dataSelectBtn(new QPushButton)
    , m_outpathSelectBtn(new QPushButton)
    , m_dlBtn(new QPushButton)
    , m_previewBtn(new QPushButton)
    , m_dataSelLabel(new QLabel)
    , m_outpathSelLabel(new QLabel)
    , m_infoLabel(new QLabel)
    , m_previewWidget(new PreviewWidget)
    , m_mediaDL(new MediaDownloader(this))
#ifdef DEV_DBG
    , m_datafile("D:/yqzd-data/json/3-1.json")
    , m_outpath("D:/yqzd-data/3-1")
#endif
{
    // this->setFixedSize(800, 800);
    m_previewWidget->setFixedSize(600, 800);

    auto *vb = new QVBoxLayout;
    vb->setContentsMargins(10, 10, 10, 10);

    m_dataSelectBtn->setText("input file");
    vb->addWidget(m_dataSelectBtn, 0, Qt::AlignLeft);
    vb->addWidget(m_dataSelLabel, 0, Qt::AlignLeft);

    m_outpathSelectBtn->setText("output path");
    vb->addWidget(m_outpathSelectBtn, 0, Qt::AlignLeft);
    vb->addWidget(m_outpathSelLabel, 0, Qt::AlignLeft);

    m_dlBtn->setText("download media");
    vb->addWidget(m_dlBtn, 0, Qt::AlignLeft);

    vb->addWidget(m_infoLabel, 0, Qt::AlignLeft);

    m_previewBtn->setText("Preview");
    vb->addWidget(m_previewBtn, 0, Qt::AlignLeft);
    vb->addStretch();

    QHBoxLayout *hb = new QHBoxLayout;
    hb->addLayout(vb);
    hb->addWidget(m_previewWidget);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(hb);
    this->setCentralWidget(centralWidget);

    connect(m_dataSelectBtn, &QPushButton::clicked,
            this, [=]() {
                m_datafile = QFileDialog::getOpenFileName(nullptr,
                                                          "Choose json data",
                                                          qApp->applicationDirPath(),
                                                          QString(),
                                                          nullptr,
                                                          QFileDialog::Option::ReadOnly);
                qDebug()<<Q_FUNC_INFO<<">>>> selected data "<<m_datafile;
                m_dataSelLabel->setText(m_datafile);
            });


    connect(m_outpathSelectBtn, &QPushButton::clicked,
            this, [=]() {
                m_outpath = QFileDialog::getExistingDirectory(nullptr,
                                                               "Choose saved path",
                                                               qApp->applicationDirPath(),
                                                               QFileDialog::ShowDirsOnly
                                                                   | QFileDialog::DontResolveSymlinks);
                qDebug()<<Q_FUNC_INFO<<">>>> selected path "<<m_outpath;
                m_outpathSelLabel->setText(m_outpath);
            });

    connect(m_dlBtn, &QPushButton::clicked,
            this, [=]() {
        m_mediaDL->download(m_datafile, m_outpath);
    });

    connect(m_previewBtn, &QPushButton::clicked,
            this, [=] {
        // auto w = new PreviewWidget;
        // w->setAttribute(Qt::WidgetAttribute::WA_DeleteOnClose);
        // w->load(m_datafile, m_outpath);
        // w->drawPage(0);
        // w->show();
        m_previewWidget->load(m_datafile, m_outpath);
        m_previewWidget->drawPage(7);
    });

    connect(m_mediaDL, &MediaDownloader::dlError,
            this, [=](const QString &msg) {
        QMessageBox::warning(nullptr, "Error", msg);
    });

    connect(m_mediaDL, &MediaDownloader::downloadState,
            this, [=](const QString &msg) {
        m_infoLabel->setText(msg);
    });

}

MainWindow::~MainWindow()
{

}




