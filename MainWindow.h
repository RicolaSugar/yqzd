#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>

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

    QLabel      *m_dataSelLabel     = nullptr;
    QLabel      *m_outpathSelLabel  = nullptr;
    QLabel      *m_infoLabel        = nullptr;

    MediaDownloader *m_mediaDL      = nullptr;

    QString m_datafile;
    QString m_outpath;

};
#endif // MAINWINDOW_H
