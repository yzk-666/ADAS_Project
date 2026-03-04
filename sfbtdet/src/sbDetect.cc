#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <chrono>
#include <vector>
#include <iostream>
#include <dirent.h> // For POSIX directory functions
#include <sys/types.h>
#include <unistd.h>
#include <cstring> // For strcmp
#include <opencv2/opencv.hpp>
#include "yolov5.h"
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"
#include <pthread.h>
#include <atomic>
#include <queue>
#include <cstdlib>

#include "mainwindow.h"
#include <QApplication>

#define MAX_FRAME_QUEUE 5 //帧队列最大容量

static pthread_mutex_t mutex;
static pthread_mutex_t audio_mutex;
static pthread_cond_t captcure_detect_cond;
static pthread_cond_t audio_broadcast_cond;
std::atomic<bool> Video_Capture_thread_exit(false); 
std::atomic<bool> Safebelt_Detect_thread_exit(false);
std::atomic<bool> Audio_broadcast(false);
std::queue<cv::Mat> frame_queue;  //创建帧队列

MainWindow *w;

void* Video_Capture_thread(void* arg)
{
    std::cout << "摄像头捕获线程开始" << std::endl;
    cv::VideoCapture cap;
    cap.open(1);
    if(!cap.isOpened())
    {
        std::cout << "摄像头打开失败" << std::endl;
        return NULL;
    }
    std::cout << "摄像头打开成功" << std::endl;
    
    cap.set(cv::CAP_PROP_FRAME_WIDTH,640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT,480);
    cap.set(cv::CAP_PROP_FPS, 30); 

    while(Video_Capture_thread_exit.load() == false)
    {
        cv::Mat frame;
        cv::Mat frame_flip;
        cap >> frame;
        if(frame.empty())continue;
        cv::flip(frame,frame_flip,1);
        pthread_mutex_lock(&mutex);
        if(frame_queue.size() >= MAX_FRAME_QUEUE)frame_queue.pop();
        frame_queue.push(frame_flip);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&captcure_detect_cond);
        std::cout << "队列数量: " << frame_queue.size() << " 张\n";
    }
    std::cout << "摄像头捕获线程结束" << std::endl;
    pthread_exit(NULL);
}

void frame_inference(cv::Mat &frame_rgb, rknn_app_context_t &safebelt_ctx)
{
    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));

    src_image.format = IMAGE_FORMAT_RGB888;
    src_image.width = frame_rgb.cols;
    src_image.height = frame_rgb.rows;
    src_image.size = frame_rgb.total() * frame_rgb.elemSize();
    src_image.virt_addr = (unsigned char*)malloc(src_image.size);
    // 复制frame_rgb的数据到分配的内存
    memcpy(src_image.virt_addr, frame_rgb.data, src_image.size);
    
    int ret;
    object_detect_result_list od_results;
    ret = inference_yolov5_model(&safebelt_ctx, &src_image, &od_results);
    
    if(ret)
    {
        std::cout << "推理图片失败" << std::endl;
        free(src_image.virt_addr);
        return;
    }
    
    std::cout << "推理图片成功!" << std::endl;

    // 画框
    for(int i = 0; i < od_results.count; i++)
    {
        object_detect_result *det_result = &(od_results.results[i]);
        draw_rectangle(&src_image,
                       det_result->box.left,
                       det_result->box.top,
                       det_result->box.right - det_result->box.left,
                       det_result->box.bottom - det_result->box.top,
                       COLOR_BLUE, 3);
        char text[128];
        char *cls_id = coco_cls_to_name(det_result->cls_id);
        sprintf(text, "%s %.1f%%", cls_id, det_result->prop * 100);
        draw_text(&src_image, text, det_result->box.left, det_result->box.top - 20, COLOR_RED, 10);
        if(strncmp(cls_id,"unbelted",8) == 0)
        {
            pthread_mutex_lock(&audio_mutex);
            Audio_broadcast = true;
            std::cout << "发送音频线程启动信号" << std::endl;
            pthread_mutex_unlock(&audio_mutex);
            pthread_cond_signal(&audio_broadcast_cond);
        }
    }
    
    cv::Mat display_frame(src_image.height, src_image.width, CV_8UC3, src_image.virt_addr);
    std::cout << "画框完毕" << std::endl;

    // 克隆图像数据
    cv::Mat display_frame_clone = display_frame.clone();
    
    free(src_image.virt_addr);
    
    // 传给Qt作显示
    cv::Mat frame_bgr;
    cv::cvtColor(display_frame_clone,frame_bgr,cv::COLOR_RGB2BGR);
    w->pushframe(frame_bgr);
    std::cout << "Qt推送了一张图片" << std::endl;
}

void* Safebelt_Detect_thread(void* arg)
{
    cv::Mat frame_detect;
    const std::string modelPath = "../model/yolov5s_best.rknn";
    int ret;
    rknn_app_context_t safebelt_ctx;
    memset(&safebelt_ctx, 0, sizeof(rknn_app_context_t));

    std::cout << "安全带检测线程开始" << std::endl;

    init_post_process();
    std::cout << "调用后处理初始化函数成功" << std::endl;

    ret = init_yolov5_model(modelPath.c_str(),&safebelt_ctx);
    if(ret)
    {
        printf("调用模型初始化函数失败\n");
        pthread_exit(NULL);
    }
    std::cout << "调用模型初始化函数成功" << std::endl;

    while(Safebelt_Detect_thread_exit.load() == false)
    {
        pthread_mutex_lock(&mutex);
        while(frame_queue.empty()) //用while避免虚假唤醒
            pthread_cond_wait(&captcure_detect_cond,&mutex);
        frame_detect = frame_queue.back().clone();
        pthread_mutex_unlock(&mutex);

        //计算处理所需时间
        auto start = std::chrono::high_resolution_clock::now();

        cv::Mat frame_rgb;
        cv::cvtColor(frame_detect,frame_rgb,cv::COLOR_BGR2RGB);

        std::cout << "获取一张检测图像" << std::endl;

        frame_inference(frame_rgb,safebelt_ctx);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "Total execution time: " << elapsed.count() << " ms\n";
    }
    
    std::cout << "安全带检测线程结束" << std::endl;
    pthread_exit(NULL);
}

void* Audio_broadcast_thread(void* arg)
{
    std::cout << "音频播报线程开启" << std::endl;
    char cmd[30];
    while(Safebelt_Detect_thread_exit.load() == false)
    {
        pthread_mutex_lock(&audio_mutex);
        std::cout << "音频线程阻塞" << std::endl;
        while(Audio_broadcast == false)
            pthread_cond_wait(&audio_broadcast_cond,&audio_mutex);
        Audio_broadcast = false;
        pthread_mutex_unlock(&audio_mutex);
        system("aplay ../audio/nosafebelt.wav");
        sleep(5);
    }
    std::cout << "音频播报线程结束" << std::endl;
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "xcb");  //设置环境变量
    
    QApplication a(argc, argv);
    w = new MainWindow();
    w->showFullScreen();

    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&audio_mutex,NULL);

    pthread_cond_init(&captcure_detect_cond,NULL);
    pthread_cond_init(&audio_broadcast_cond,NULL);

    pthread_t Video_Capture_thread_tid;
    pthread_t Safebelt_Detect_thread_tid;
    pthread_t Audio_broadcast_thread_tid;

    pthread_create(&Video_Capture_thread_tid,NULL,Video_Capture_thread,NULL);
    pthread_create(&Safebelt_Detect_thread_tid,NULL,Safebelt_Detect_thread,NULL);
    pthread_create(&Audio_broadcast_thread_tid,NULL,Audio_broadcast_thread,NULL);

    int ret = a.exec();

    pthread_join(Video_Capture_thread_tid,NULL);
    pthread_join(Safebelt_Detect_thread_tid,NULL);
    pthread_join(Audio_broadcast_thread_tid,NULL);

    return ret;
}