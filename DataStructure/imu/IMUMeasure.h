#ifndef IMUMEASURE_H
#define IMUMEASURE_H

#include <deque>
#include <memory>

#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <sophus/se3.hpp>

#include "DataStructure/Measurements.h"

struct IMUData {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    Eigen::Vector3d           acceleration;
    Eigen::Vector3d           gyroscopes;
    Eigen::Quaternion<double> orientation;
    Eigen::Vector3d           geomagnetism;
    double                    atmospheric_pressure;
    okvis::Time               timeStamp;
};

struct IMUMeasure : public MeasurementBase<IMUData> {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    IMUMeasure(int sensorId, const okvis::Time& timeStamp, const Eigen::Vector3d& acceleration, const Eigen::Vector3d& gyroscopes,
               Eigen::Vector3d geomagnetism = Eigen::Vector3d(), Eigen::Quaternion<double> orientation = Eigen::Quaternion<double>()) {
        this->sensorId = sensorId;
        this->timeStamp = timeStamp;
        this->measurement.acceleration = acceleration;
        this->measurement.gyroscopes = gyroscopes;
        this->measurement.geomagnetism = geomagnetism;
        this->measurement.orientation = orientation;
        this->measurement.timeStamp = timeStamp;
    }

    typedef std::shared_ptr<IMUMeasure>                                  IMUMeasure_Ptr;
    typedef Sophus::SE3d                                                 Transformation;
    typedef Eigen::Matrix<double, 9, 1>                                  SpeedAndBias;    ///< speed, b_g, b_a
    typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>        covariance_t;
    typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>        jacobian_t;
    typedef Eigen::Matrix<double, Eigen::Dynamic, 1>                     Error_t;

    class ImuMeasureDeque : public std::deque<std::shared_ptr<IMUMeasure>> {
    public:
        bool addImuMeasurement(int sensorID,
                               const okvis::Time & stamp,
                               const Eigen::Vector3d & alpha,
                               const Eigen::Vector3d & omega);
    };
};


struct ImuParameters {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    IMUMeasure::Transformation T_BS; ///< Transformation from Body frame to IMU (sensor frame S).
    double a_max;  ///< Accelerometer saturation. [m/s^2]
    double g_max;  ///< Gyroscope saturation. [rad/s]
    double sigma_g_c;  ///< Gyroscope noise density.
    double sigma_bg;  ///< Initial gyroscope bias.
    double sigma_a_c;  ///< Accelerometer noise density.
    double sigma_ba;  ///< Initial accelerometer bias
    double sigma_gw_c; ///< Gyroscope drift noise density.
    double sigma_aw_c; ///< Accelerometer drift noise density.
    double tau;  ///< Reversion time constant of accerometer bias. [s]
    double g;    ///< Earth acceleration.
    Eigen::Vector3d a0;  ///< Mean of the prior accelerometer bias.
    int rate;  ///< IMU rate in Hz.
};




#endif // IMUMEASURE_H