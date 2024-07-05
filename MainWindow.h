#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QSlider>

class PreviewWidget;
class MediaDownloader;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    QPushButton *m_dataSelectBtn    = nullptr;
    QPushButton *m_outpathSelectBtn = nullptr;
    QPushButton *m_dlBtn            = nullptr;
    QPushButton *m_previewBtn       = nullptr;
    QPushButton *m_nextBtn          = nullptr;
    QPushButton *m_previousBtn      = nullptr;
    QSlider     *m_slider           = nullptr;


    QLabel      *m_dataSelLabel     = nullptr;
    QLabel      *m_outpathSelLabel  = nullptr;
    QLabel      *m_infoLabel        = nullptr;

    PreviewWidget *m_previewWidget  = nullptr;

    MediaDownloader *m_mediaDL      = nullptr;

    int     m_curPageNum            = 0;

    QString m_datafile;
    QString m_outpath;

};
#endif // MAINWINDOW_H
