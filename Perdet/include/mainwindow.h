#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <opencv2/opencv.hpp>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QMutex>
#include <QDesktopWidget>
#include <QApplication>
#include <QVBoxLayout>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void pushframe(cv::Mat frame);
    std::queue<cv::Mat> qt_frame_queue;
    
private:
    QLabel *labelImage;
    QLabel *labeltest;
    QTimer *timer;
    QElapsedTimer fpsTimer;
    int framecount = 0;
    int fps = 0;

public slots:
    void DisplayFrame();
};
#endif // MAINWINDOW_H
