//
// Created by zhangzhenwei on 1/17/17.
//

#include <memory>
#include <opencv2/ts.hpp>

#include "IO/camera/CameraIO.h"
#include "DataStructure/cv/Feature.h"
#include "cv/FeatureDetector/EdgeDetector.h"

TEST(edge_detector, edge_detector) {
    std::string camDatafile = "../testData/mav0/cam1/data.csv";
    std::string camParamfile ="../testData/mav0/cam1/sensor.yaml";
    CameraIO camTest(camDatafile,camParamfile);
    const CameraIO::pCamereParam cam = camTest.getCamera();
    cv::Mat pic = cv::imread("../testData/mav0/cam0/data/1403715273562142976.png",0);
    cv::Mat pic1 = cv::imread("../testData/mav0/cam0/data/1403715310362142976.png",0);
    cv::imshow("src",pic);
    cv::imshow("src1", pic1);

    cv::Mat picCanny; cv::Canny(pic,picCanny,100,150);
    cv::Mat picCanny1; cv::Canny(pic1,picCanny1,100,150);

    cv::imshow("Edge0",picCanny);
    cv::imshow("Edge01",picCanny1);
    GTEST_ASSERT_NE(pic.empty(), true);
    std::shared_ptr<cvFrame> frame = std::make_shared<cvFrame>(cam, pic);
    std::shared_ptr<cvFrame> frame1 = std::make_shared<cvFrame>(cam, pic1);

    GTEST_ASSERT_NE(frame.get(), nullptr);
    feature_detection::AbstractDetector *detector = new feature_detection::EdgeDetector(pic.cols, pic.rows, 25, IMG_LEVEL);
    GTEST_ASSERT_NE(detector, nullptr);

    feature_detection::AbstractDetector::features_t features, features1;
    features.clear(); features1.clear();

#define SHOW_EDGE
#ifdef  SHOW_EDGE
    detector->detect(frame, frame->getMeasure().measurement.imgPyr, 5,features);
    detector->detect(frame1, frame1->getMeasure().measurement.imgPyr, 5, features1);
    GTEST_ASSERT_NE(features.empty(), true);


    cv::Mat result = cv::imread("../testData/mav0/cam0/data/1403715273562142976.png");
    cv::Mat result1 = cv::imread("../testData/mav0/cam0/data/1403715310362142976.png");
    //    int count = 0, count1 = 0;
    int levelTimes[5] = {1,2,4,8,16};

    cv::Scalar color[3] = {cv::Scalar(255,0,0),cv::Scalar(255,255,0),cv::Scalar(0,0,255)};
    for (auto it : features) {
        cv::Point2i point0((it)->px(0)*levelTimes[it->level]+it->grad(1)*0.75, (it)->px(1)*levelTimes[it->level]-it->grad(0)*0.75);
        cv::Point2i point1((it)->px(0)*levelTimes[it->level]-it->grad(1)*0.75, (it)->px(1)*levelTimes[it->level]+it->grad(0)*0.75);
        cv::line(result, point0, point1, color[it->level]);
    }
    for (auto it : features1) {
        cv::Point2i point0((it)->px(0)*levelTimes[it->level]+it->grad(1)*0.75, (it)->px(1)*levelTimes[it->level]-it->grad(0)*0.75);
        cv::Point2i point1((it)->px(0)*levelTimes[it->level]-it->grad(1)*0.75, (it)->px(1)*levelTimes[it->level]+it->grad(0)*0.75);
        cv::line(result1, point0, point1, color[it->level]);
    }

    //    std::cout<<"non zero level: "<<count<<" "<<count1<<"\n";
    cv::imshow("result",result);
    cv::imshow("result1", result1);
    cv::waitKey();

#else
    detector->detect(frame1, frame1->getMeasure().measurement.imgPyr, 5, features1);
    GTEST_ASSERT_NE(features1.empty(), true);

#endif
    printf("file: %s,  lines: %d\n",__FILE__, __LINE__);

}

