#include "cvFrame.h"

cvMeasure& cvFrame::getMeasure() {
    return cvData;
}

bool cvFrame::checkOccupy(int u, int v) {
    return occupy[u + v * detectWidthGrid * detectCellHeight];
}

void cvFrame::setCellTrue(int u, int v) {
    cell[u + v * detectCellWidth] = true;
    int ui = u * detectWidthGrid;
    int vi = v * detectHeightGrid;
    int us = ui + detectCellWidth;
    int vs = vi + detectHeightGrid;
    for(int i = ui; i < us; ++i) {
        for(int j = vi; j < vs; ++j)
            occupy[i + j * detectCellWidth * detectWidthGrid] = true;
    }
}

const cvFrame::Pic_t& cvFrame::getPicture() {
    return cvData.measurement.pic;
}

int cvFrame::getWidth(int level) {
    return cvData.measurement.width[level];
}

int cvFrame::getHeight(int level) {
    return cvData.measurement.height[level];
}

double cvFrame::getIntensity(int u, int v, int level) {
    if(u < 0 || v < 0 || level > IMG_LEVEL)
        return -1.0;

    int rows = cvData.measurement.height[level];
    int cols = cvData.measurement.width[level];

    if(u >= cols || v >= rows)
        return -1.0;

    return cvData.measurement.imgPyr[level][v * cols + u][0];
}

double cvFrame::getIntensityBilinear(double u, double v, int level) {
    int rows = cvData.measurement.height[level];
    int cols = cvData.measurement.width[level];

    if(u >= cols || v >= rows)
        return -1.0;

    int ui = int(std::floor(u));    int vi = int(std::floor(v));
    double ud = u - ui;        double vd = v - vi;
//    printf("u: %lf, v: %lf\n",u,v);
    double leftTop     = cvData.measurement.imgPyr[level][vi * cols + ui](0);
    double rightTop    = cvData.measurement.imgPyr[level][vi * cols + ui + 1](0);
    double leftBottom  = cvData.measurement.imgPyr[level][vi * cols + cols + ui](0);
    double rightBottom = cvData.measurement.imgPyr[level][vi * cols + cols + ui + 1](0);

    double row1 = leftTop * (1 - ud) + ud * rightTop;
    double row2 = leftBottom * (1 - ud) + ud * rightBottom;
//    printf("row1: %lf,row2: %lf,ud: %lf,vd: %lf; lt %lf,rt %lf,lb %lf,rb %lf\n",row1,row2,ud,vd,leftTop,rightTop,leftBottom,rightBottom);
    return row1 * (1 - vd) + row2 * vd;
}

Eigen::Vector2d cvFrame::getGradBilinear(double u, double v, int level){
    int rows = cvData.measurement.height[level];
    int cols = cvData.measurement.width[level];

    if(u >= cols || v >= rows)
        return Eigen::Vector2d::Zero();

    int ui = int(std::floor(u));    int vi = int(std::floor(v));
    double ud = u - ui;        double vd = v - vi;

    const Eigen::Vector2d& leftTop     = cvData.measurement.imgPyr[level][vi * cols + ui].block<2, 1>(1, 0);
    const Eigen::Vector2d& rightTop    = cvData.measurement.imgPyr[level][vi * cols + ui + 1].block<2, 1>(1, 0);
    const Eigen::Vector2d& leftBottom  = cvData.measurement.imgPyr[level][vi * cols + cols + ui].block<2, 1>(1, 0);
    const Eigen::Vector2d& rightBottom = cvData.measurement.imgPyr[level][vi * cols + cols + ui + 1].block<2, 1>(1, 0);

    Eigen::Vector2d row1 = leftTop * (1 - ud) + ud * rightTop;
    Eigen::Vector2d row2 = leftBottom * (1 - ud) + ud * rightBottom;

    return row1 * (1 - vd) + row2 * vd;
}


bool cvFrame::getGrad(int u, int v, cvFrame::grad_t&  out, int level) {
    if(u < 0 || v < 0 || level > IMG_LEVEL)
        return false;

    int rows = cvData.measurement.height[level];
    int cols = cvData.measurement.width[level];

    if(u >= cols || v >= rows)
        return false;

    out =  cvData.measurement.imgPyr[level][v * cols + u].segment<2>(1);
    return true;
}

cvFrame::cvFrame(const std::shared_ptr<AbstractCamera> &cam, Pic_t &pic, okvis::Time time) {
    cam_ = cam;
    //pose_ = Sophus::SE3d::exp(Eigen::Matrix<double, 6, 1>::Zero());
    cvData.measurement.pic = pic;
    int rows = pic.rows;
    int cols = pic.cols;
    cvData.timeStamp = time;
    // 341 = 1 + 4 + 16 + 64 + 256 !>> cell's numbel for each level
    // for a point(u,v) in cell(on the l level): (u,v,l) , occupy[(4^l-1)/3 + v*2^l + u]
    memset(occupy, 0, sizeof(bool) * detectCellWidth * detectCellHeight * detectHeightGrid * detectWidthGrid);
    memset(cell, 0, sizeof(bool) * detectCellWidth * detectCellHeight);
    for(int i = 0; i < IMG_LEVEL; ++i) {
        if(i != 0) {
            rows /= 2;
            cols /= 2;
        }

        cvData.measurement.width[i]  = cols;
        cvData.measurement.height[i] = rows;
        cvData.measurement.imgPyr[i].assign(cvData.measurement.width[i] * cvData.measurement.height[i], Eigen::Vector3d::Zero());
        cvData.measurement.gradNormPyr[i].assign(cvData.measurement.width[i] * cvData.measurement.height[i], 0.0);

        if(i == 0) {
            for(int p = 0; p < rows; p++) {
                for(int q = 0; q < cols; q++) {
                    cvData.measurement.imgPyr[i][p * cols + q][0] = double(pic.at<u_char>(p, q));
                }
            }
        }

        else {
            for(int p = 0; p < rows; p++) {
                for(int q = 0; q < cols; q++)
                    cvData.measurement.imgPyr[i][p * cols + q][0]
                            = 0.25 * (  cvData.measurement.imgPyr[i - 1][p * cols * 4          + q * 2][0]
                                      + cvData.measurement.imgPyr[i - 1][p * cols * 4          + q * 2 + 1][0]
                                      + cvData.measurement.imgPyr[i - 1][p * cols * 4 + 2*cols + q * 2 + 1][0]
                                      + cvData.measurement.imgPyr[i - 1][p * cols * 4 + 2*cols + q * 2][0]);
            }
        }

        for(int p = 1; p < rows - 1; ++p) {
            for (int q = 1; q < cols - 1; ++q) {
                cvData.measurement.imgPyr[i][p * cols + q][1]
                        = 0.5 * (cvData.measurement.imgPyr[i][p * cols + q + 1][0]
                               - cvData.measurement.imgPyr[i][p * cols + q - 1][0]);
                cvData.measurement.imgPyr[i][p * cols + q][2]
                        = 0.5 * (cvData.measurement.imgPyr[i][p * cols + q + cols][0]
                               - cvData.measurement.imgPyr[i][p * cols + q - cols][0]);

                cvData.measurement.gradNormPyr[i][p * cols + q]
                        = std::sqrt(cvData.measurement.imgPyr[i][p * cols + q][1] * cvData.measurement.imgPyr[i][p * cols + q][1]
                                  + cvData.measurement.imgPyr[i][p * cols + q][2] * cvData.measurement.imgPyr[i][p * cols + q][2]);

            }
        }
    }
}

double cvFrame::getGradNorm(int u, int v, int level) {
    if(u < 0 || v < 0 || level > IMG_LEVEL)
        return -1.0;

    int rows = cvData.measurement.height[level];
    int cols = cvData.measurement.width[level];

    if(u >= cols || v >= rows)
        return -1.0;

    return cvData.measurement.gradNormPyr[level][v * cols + u];
}

cvFrame::~cvFrame() {

}

const std::shared_ptr<Feature>& cvFrame::addFeature(const std::shared_ptr<Feature>& ft) {
    cvData.fts_.push_back(ft);
    return ft;
}

int cvFrame::getID() {
    return cvData.id;
}

const okvis::Time &cvFrame::getTimestamp() {
    return cvData.timeStamp;
}

int cvFrame::getSensorID() {
    return cvData.sensorId;
}

const cvFrame::cam_t& cvFrame::getCam() {
    return cam_;
}

const cvFrame::cov_t &cvFrame::getCovariance() {
    return Cov_;
}

pose_t cvFrame::getPose() {
    pose_mutex.lock_shared();
    auto pose = pose_;
    pose_mutex.unlock_shared();
    return pose_;
}

void cvFrame::setPose(pose_t &pose) {
    pose_mutex.lock();
    pose_ = pose;
    pose_mutex.unlock();
}

void cvFrame::setCovariance(cvFrame::cov_t &Cov) {
    Cov_ = Cov;
}

bool cvFrame::isKeyframe() {
    return is_keyframe_ == true;
}

void cvFrame::setKey() {
    is_keyframe_ = true;
}

void cvFrame::cancelKeyframe() {
    is_keyframe_ = false;
}

int cvFrame::getPublishTimestamp() {
    return last_published_ts_;
}

const cvFrame::position_t cvFrame::pos() {
    pose_mutex.lock_shared();
    auto position = pose_.translation();
    pose_mutex.unlock_shared();
    return position;
}

