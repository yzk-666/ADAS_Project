#include "mainwindow.h"

#define MAX_FRAME_QUEUE 5

QMutex mutex;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
int screenWidth_total = QApplication::desktop()->screenGeometry().width();
    int screenHeight_total = QApplication::desktop()->screenGeometry().height();

    //setWindowTitle("ADAS");
    setFixedSize(screenWidth_total/2,screenHeight_total);
    setWindowFlags(Qt::FramelessWindowHint);

    labelImage = new QLabel(this);
    labelImage->setAlignment(Qt::AlignCenter);
    labelImage->setScaledContents(true);
    labelImage->setFixedSize(screenWidth_total/2,screenHeight_total);
    
    this->move(0,0);

    timer = new QTimer(this);
    timer->setInterval(33);
    connect(timer,&QTimer::timeout,this,&MainWindow::DisplayFrame);
    timer->start();
    fpsTimer.start();
}

MainWindow::~MainWindow()
{
    
}

void MainWindow::pushframe(cv::Mat frame)
{
    QMutexLocker locker(&mutex);
    if(qt_frame_queue.size() >= MAX_FRAME_QUEUE)qt_frame_queue.pop();
    qt_frame_queue.push(frame.clone());
}

void MainWindow::DisplayFrame()
{
    cv::Mat frame_bgr;
    cv::Mat frame_rgb;
    {
        QMutexLocker locker(&mutex);
        if(!qt_frame_queue.empty())
        {
            frame_bgr = qt_frame_queue.back();
        }
    }
    if(qt_frame_queue.empty())return;

    //计算帧率
    framecount++;
    if(fpsTimer.elapsed() >= 1000)
    {
        fps = framecount * 1000 / fpsTimer.elapsed();
        framecount = 0;
        fpsTimer.restart();
    }
    std::string fps_text = "FPS:" + std::to_string(fps);
    cv::putText(frame_bgr,
                fps_text,
                cv::Point(10,30),
                cv::FONT_HERSHEY_SIMPLEX,
                0.7,
                cv::Scalar(0, 255, 0),
                2);

    cv::cvtColor(frame_bgr,frame_rgb,cv::COLOR_BGR2RGB);
    QImage qimg(frame_rgb.data,
                frame_rgb.cols,
                frame_rgb.rows,
                frame_rgb.step,
                QImage::Format_RGB888);
    labelImage->setPixmap(QPixmap::fromImage(qimg));

    std::cout << "Qt作显示" << std::endl;
}