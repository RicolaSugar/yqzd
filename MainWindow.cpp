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
    , m_nextBtn(new QPushButton)
    , m_previousBtn(new QPushButton)
    , m_saveBtn(new QPushButton)
    , m_slider(new QSlider(Qt::Orientation::Horizontal))
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

    m_slider->setTickPosition(QSlider::TicksBothSides);
    m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

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

    m_nextBtn->setText("Next >>");
    vb->addWidget(m_nextBtn, 0, Qt::AlignLeft);

    m_previousBtn->setText("Previous <<");
    vb->addWidget(m_previousBtn, 0, Qt::AlignLeft);
    vb->addStretch();

    m_saveBtn->setText("Save images");
    vb->addWidget(m_saveBtn, 0, Qt::AlignLeft);
    vb->addStretch();

    QHBoxLayout *hb = new QHBoxLayout;
    hb->addLayout(vb);
    hb->addWidget(m_previewWidget);

    QVBoxLayout *vv = new QVBoxLayout;
    vv->addWidget(m_slider);
    vv->addLayout(hb);

    auto centralWidget = new QWidget;
    centralWidget->setLayout(vv);
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
        // m_previewWidget->load(m_datafile, m_outpath);
        // m_previewWidget->drawPage(30);
        if (m_previewWidget->load(m_datafile, m_outpath)) {
            m_slider->setMaximum(m_previewWidget->pageCount());
        }
    });

    connect(m_nextBtn, &QPushButton::clicked,
            this, [=]() {
        if (m_curPageNum == (m_previewWidget->pageCount()-1)) {
            return;
        }
        m_curPageNum++;
        m_previewWidget->drawPage(m_curPageNum);
        m_slider->setValue(m_curPageNum);
    });

    connect(m_previousBtn, &QPushButton::clicked,
            this, [=]() {
        if (m_curPageNum == 0) {
            return;
        }
        m_curPageNum--;
        m_previewWidget->drawPage(m_curPageNum);
        m_slider->setValue(m_curPageNum);
    });

    connect(m_mediaDL, &MediaDownloader::dlError,
            this, [=](const QString &msg) {
        QMessageBox::warning(nullptr, "Error", msg);
    });

    connect(m_mediaDL, &MediaDownloader::downloadState,
            this, [=](const QString &msg) {
        m_infoLabel->setText(msg);
    });

    connect(m_slider, &QSlider::valueChanged,
            this, [=](int value) {
        if (m_curPageNum != value) {
            m_curPageNum = value;
            m_previewWidget->drawPage(value);
        }
        m_infoLabel->setText(QLatin1StringView("Page at ") + QString::number(value));
    });

    connect(m_saveBtn, &QPushButton::clicked,
            this, [=]() {
        if (m_previewWidget->load(m_datafile, m_outpath)) {
            for (int i=0; i<m_previewWidget->pageCount(); ++i) {
                m_infoLabel->setText(QLatin1StringView("Render page ") + QString::number(i));
                qApp->processEvents();
                m_previewWidget->save(i, m_outpath + "/out");
                qApp->processEvents();
            }
        }
    });

}

MainWindow::~MainWindow()
{

}




