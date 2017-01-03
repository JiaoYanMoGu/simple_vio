#ifndef CAMERAIO_H
#define CAMERAIO_H
#include <map>
#include <memory>
#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <opencv2/opencv.hpp>

#include "../IOBase.h"
#include "../../DataStructure/cv/cvFrame.h"

class CameraIO : public IOBase<cvMeasure> {
public:
    typedef std::shared_ptr<AbstractCamera>        pCamereParam;
    typedef std::map<double, cv::Mat>              CameraData;
public:
    CameraIO(std::string imageFile, std::string cameraParamfile);
    const pCamereParam& getCamera();

/*
    pose_t          getTBS(void) {return  camParam->getTBS();}
    int             getRate(void) {return  camParam->getRate();}
    std::string     getCameraMode(void) {return  camParam->getCameraMode();}
    std::string     getDistorMode(void) {return  camParam->getDistorMode();}
*/
private:
    pCamereParam        camParam;
    CameraData          camData;
};

#endif // CAMERAIO_H
