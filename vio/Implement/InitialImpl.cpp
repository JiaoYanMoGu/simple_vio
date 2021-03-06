//
// Created by lancelot on 1/17/17.
//

#include <limits>
#include <ceres/ceres.h>

#include "InitialImpl.h"
#include "DataStructure/cv/cvFrame.h"
#include "DataStructure/imu/imuFactor.h"
#include "util/util.h"
#include "DataStructure/cv/Point.h"
#include "DataStructure/cv/Feature.h"

class CERES_EXPORT SO3Parameterization : public ceres::LocalParameterization {
public:
	virtual ~SO3Parameterization() {}

	virtual bool Plus(const double *x,
	                  const double *delta,
	                  double *x_plus_delta) const;

	virtual bool ComputeJacobian(const double *x,
	                             double *jacobian) const;

	virtual int GlobalSize() const { return 3; }

	virtual int LocalSize() const { return 3; }
};

bool SO3Parameterization::ComputeJacobian(const double *x, double *jacobian) const {
	ceres::MatrixRef(jacobian, 3, 3) = ceres::Matrix::Identity(3, 3);
	return true;
}

bool SO3Parameterization::Plus(const double *x,
                               const double *delta,
                               double *x_plus_delta) const {
	Eigen::Vector3d origin_x, delta_x;
	for (int i = 0; i < 3; ++i) {
		origin_x(i) = x[i];
		delta_x(i) = delta[i];
	}

	Sophus::SO3d R = Sophus::SO3d::exp(origin_x);
	Sophus::SO3d delta_R = Sophus::SO3d::exp(delta_x);
	Eigen::Matrix<double, 3, 1> x_plus_delta_lie = (R * delta_R).log();

	for (int i = 0; i < 3; ++i) x_plus_delta[i] = x_plus_delta_lie(i, 0);
	return true;

}


class gbiasErr : public ceres::SizedCostFunction<3, 3> {
public:
	gbiasErr(std::shared_ptr<viFrame> viframe_1, std::shared_ptr<viFrame> viframe_2,
	         const IMUMeasure::Transformation &deltaPose, const imuFactor::FacJBias_t &facJac)
			: deltaPose_(deltaPose), facJac_(facJac) {
		this->viframe_1 = viframe_1;
		this->viframe_2 = viframe_2;
	}

	virtual ~gbiasErr() {}

	virtual bool Evaluate(double const *const *parameters,
	                      double *residuals,
	                      double **jacobians) const {

		Eigen::Map<const Eigen::Vector3d> gbias(parameters[0]);
		Eigen::Vector3d err = Sophus::SO3d::log(
				(deltaPose_.so3() * Sophus::SO3d::exp(facJac_.block<3, 3>(0, 0) * gbias)).inverse()
				* viframe_2->getPose().so3() * viframe_1->getPose().so3().inverse());

		Eigen::Matrix3d Jac = -leftJacobian(err).inverse() * facJac_.block<3, 3>(0, 0);

		int k = 0;
		for (int i = 0; i < 3; ++i) {
			residuals[i] = err(i);
			if (jacobians && jacobians[0]) {
				for (int j = 0; j < 3; ++j)
					jacobians[0][k++] = Jac(i, j);
			}
		}

		return true;
	}

private:
	std::shared_ptr<viFrame> viframe_1;
	std::shared_ptr<viFrame> viframe_2;
	const IMUMeasure::Transformation &deltaPose_;
	const imuFactor::FacJBias_t &facJac_;
};

InitialImpl::InitialImpl() {
	imu_ = std::make_shared<IMU>();
}

bool InitialImpl::init(std::vector<std::shared_ptr<viFrame>> &VecFrames,
                       std::vector<std::shared_ptr<imuFactor>> &VecImuFactor,
                       std::shared_ptr<ImuParameters> &imuParam, int n_iter) {
	assert(VecImuFactor.size() + 1 == VecFrames.size());

	const double convergeErr = 0.03 * 0.03;

	size_t size = VecFrames.size();

	if (size < 3) {
		printf("frames are too few\n");
		return false;
	}

	if (size < 6) {
		printf("warning: the information got from initializing steps may not be ensured!\n");
	}

	//! esitmate gbias;
	double gbias_[3] = {0.0, 0.0, 0.0};
	ceres::Problem problem;
	for (int i = 0; i < size - 1; ++i) {
		const IMUMeasure::Transformation &deltaPose = VecImuFactor[i]->deltaPose;
		const imuFactor::FacJBias_t &facJac = VecImuFactor[i]->getJBias();
		problem.AddResidualBlock(new gbiasErr(VecFrames[i], VecFrames[i + 1], deltaPose, facJac),
		                         new ceres::HuberLoss(0.5), gbias_);
	}
	problem.SetParameterization(gbias_, new SO3Parameterization());

	ceres::Solver::Options options;
	ceres::Solver::Summary summary;

	options.max_num_iterations = n_iter;
	options.minimizer_type = ceres::TRUST_REGION;
	options.linear_solver_type = ceres::DENSE_QR;
	ceres::Solve(options, &problem, &summary);

	if (summary.termination_type != ceres::CONVERGENCE)
		return false;

	Eigen::Map<Eigen::Vector3d> gbias(gbias_);

	for (size_t i = 0; i < VecFrames.size(); ++i)
		VecFrames[i]->getSpeedAndBias().block<3, 1>(3, 0) = gbias;

	//! estimate scale and gravity; refine bias_a, scale, gravity

	double G = imuParam->g.norm();
	Eigen::Vector3d cP_B = dynamic_cast<VIOPinholeCamera *>(VecFrames[0]->cvframe->getCam().get())->getT_BS().inverse().translation();
	Eigen::Vector3d g_w = imuParam->g;
	Eigen::MatrixXd A(3 * (size - 2), 4);
	Eigen::MatrixXd B(3 * (size - 2), 1);
	Eigen::MatrixXd C(3 * (size - 2), 6);
	Eigen::MatrixXd D(3 * (size - 2), 1);
	Eigen::Vector3d p1 = VecFrames[0]->cvframe->getPose().translation();
	Eigen::Vector3d p2;
	Eigen::Vector3d p3;
	Sophus::SO3d R1_wc = VecFrames[0]->cvframe->getPose().so3().inverse();
	Sophus::SO3d R2_wc = VecFrames[1]->cvframe->getPose().so3().inverse();
	Sophus::SO3d R3_wc = VecFrames[2]->cvframe->getPose().so3().inverse();
	Sophus::SO3d R1_wb = VecFrames[0]->getPose().so3().inverse();
	Sophus::SO3d R2_wb = VecFrames[1]->getPose().so3().inverse();
	Sophus::SO3d R3_wb = VecFrames[2]->getPose().so3().inverse();
	double dt12 = (VecFrames[1]->getTimeStamp() - VecFrames[0]->getTimeStamp()).toSec();
	p2 = scale * p1 + VecFrames[0]->spbs.block<3, 1>(0, 0) * dt12 + 0.5 * g_w * dt12 * dt12 +
	     R1_wb * VecImuFactor[0]->getPoseFac().translation() + (R1_wc.matrix() - R2_wc.matrix()) * cP_B;

	double dt23 = (VecFrames[2]->getTimeStamp() - VecFrames[1]->getTimeStamp()).toSec();
	p3 = scale * p2 + VecFrames[1]->spbs.block<3, 1>(0, 0) * dt23 + 0.5 * g_w * dt23 * dt23 +
	     R2_wb * VecImuFactor[1]->getPoseFac().translation() + (R2_wc.matrix() - R3_wc.matrix()) * cP_B;

	double t_ij = dt12;

	for (int i = 1; i < size - 1; ++i) {
		if (1 != i) {
			p1 = p2;
			p2 = p3;
			R1_wb = R2_wb;
			R2_wb = R3_wb;
			R3_wb = VecFrames[i + 1]->getPose().so3().inverse();
			R1_wc = R2_wc;
			R2_wc = R3_wc;
			R3_wc = VecFrames[i + 1]->cvframe->getPose().so3().inverse();
			dt12 = dt23;
			dt23 = (VecFrames[i + 1]->getTimeStamp() - VecFrames[i]->getTimeStamp()).toSec();
			p3 = scale * p2 + VecFrames[i]->spbs.block<3, 1>(0, 0) * dt23 + 0.5 * g_w * dt23 * dt23 +
			     R2_wb * VecImuFactor[1]->getPoseFac().translation() + (R2_wc.matrix() - R3_wc.matrix()) * cP_B;
		}
		t_ij += dt12;
		C.block<3, 1>(3 * i - 3, 0) = A.block<3, 1>(3 * i - 3, 0) = (p2 - p1) * dt23 - (p3 - p2) * dt12; //< !lambda
		A.block<3, 3>(3 * i - 3, 1) = 0.5 * Eigen::Matrix3d::Identity() * (dt12 * dt12 * dt23 - dt23 * dt23 * dt12);
		B.block<3, 1>(3 * i - 3, 0) = (R2_wc.matrix() - R1_wc.matrix()) * cP_B * dt23
		                              - (R3_wc.matrix() - R2_wc.matrix()) * cP_B * dt12
		                              + R2_wb * VecImuFactor[i]->getPoseFac().translation() * dt12
		                              + R1_wb * VecImuFactor[i - 1]->getSpeedFac() * dt12 * dt23
		                              - R1_wb * VecImuFactor[i - 1]->getPoseFac().translation() * dt23;

		C.block<3, 3>(3 * i - 3, 3) = R2_wb.matrix() * VecImuFactor[i]->getJBias().block<3, 3>(9, 0) * dt12
		                              + R1_wb.matrix() * VecImuFactor[i]->getJBias().block<3, 3>(3, 0) * dt12 * dt23
		                              - R1_wb.matrix() * VecImuFactor[i - 1]->getJBias().block<3, 3>(9, 0) * dt23;


		C(3 * i - 3, 1) = 0.5 * G * (dt12 * dt12 * dt23 + dt23 * dt23 * dt12);
		C(3 * i - 3, 2) = 0.5 * G /*dt_ij^2 */ ;
		D.block<3, 1>(3 * i - 3, 0) = (R2_wc.matrix() - R1_wc.matrix()) * cP_B * dt23
		                              - (R3_wc.matrix() - R2_wc.matrix()) * cP_B * dt12
		                              + R2_wb * VecImuFactor[i]->getPoseFac().translation() * dt12
		                              + R1_wb * VecImuFactor[i - 1]->getSpeedFac() * dt12 * dt23
		                              - R1_wb * VecImuFactor[i - 1]->getPoseFac().translation() * dt23;
		dt12 = dt23;
		dt23 = (VecFrames[i + 1]->getTimeStamp() - VecFrames[i]->getTimeStamp()).toSec();
	}

	//!< s_g : scale(1*1) , g_w(3*1) : 4*1
	Eigen::Vector4d s_g = A.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(B);
	scale = s_g(0, 0);
	std::cout << "scale approx = " << scale << std::endl;
	Eigen::Vector3d g_I = Eigen::Vector3d(0, 0, -1);
	Eigen::Vector3d g_what = s_g.block<3, 1>(1, 0);
	g_w = g_what;
	g_what.normalize();
	Eigen::Vector3d vhat = crossMx(g_I) * g_what;
	vhat.normalize();
	double theta = std::atan2((crossMx(g_I) * g_what).norm(), g_I.dot(g_what));

	Sophus::SO3d R_WI = Sophus::SO3d::exp(theta * vhat);
	Eigen::Matrix3d g_I_hat = Sophus::SO3d::hat(Eigen::Vector3d(0, 0, -1));
	for (int i = 1; i < size - 1; ++i) {
		C.block<3, 2>(3 * i - 3, 1) = (C(3 * i - 3, 1) * R_WI.matrix() * g_I_hat).block<3, 2>(0, 1);
		D.block<3, 1>(3 * i - 3, 0) += R_WI * Eigen::Vector3d(0, 0, -1) * C(3 * i - 3, 2) * t_ij * t_ij;
	}

	Eigen::Matrix<double, 6, 1> s_dxy_ba = C.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(D);
	g_w = (R_WI * Sophus::SO3d::exp(Eigen::Vector3d(s_dxy_ba(1, 0), s_dxy_ba(2, 0), 0))) * g_I * G;
	imuParam->g = g_w;

	scale = s_dxy_ba(0, 0);
	std::cout << "scale = " << scale << std::endl;
	std::set<std::shared_ptr<Point>> points;
	for (size_t i = 0; i < VecFrames.size(); ++i) {
		VecFrames[i]->spbs.block<3, 1>(6, 0) = s_dxy_ba.block<3, 1>(3, 0);
		for (auto &ft : VecFrames[i]->getCVFrame()->getMeasure().fts_) {
			auto res = points.insert(ft->point);
			if (res.second) {
				if(ft->point->n_succeeded_reproj_ < 3) {
					auto &obs = ft->point->obs_;

				}
				//std::cout << ft->point->pos_[2] << std::endl;
				ft->point->pos_ = scale * ft->point->pos_;

			}
		}
	}

	{  //! estimate velocity
		Eigen::Vector3d p1 = VecFrames[0]->cvframe->getPose().translation();
		Eigen::Vector3d p2 = VecFrames[1]->cvframe->getPose().translation();
		double dt12 = (VecFrames[1]->getTimeStamp() - VecFrames[0]->getTimeStamp()).toSec();
		Eigen::Matrix<double, 3, 1> delta_theta;
		delta_theta << s_dxy_ba(1, 0), s_dxy_ba(2, 0), 0.0;

		Sophus::SO3d R1_wc = VecFrames[0]->cvframe->getPose().so3().inverse();
		Sophus::SO3d R2_wc = VecFrames[1]->cvframe->getPose().so3().inverse();

		for (int i = 1; i < size - 1; ++i) {
			Sophus::SO3d R1_wb = VecFrames[i - 1]->getPose().so3().inverse();
			if (i != 1) {
				dt12 = (VecFrames[i]->getTimeStamp() - VecFrames[i - 1]->getTimeStamp()).toSec();
				p1 = p2;
				p2 = VecFrames[i]->cvframe->getPose().translation();
				R1_wc = R2_wc;
				R1_wc = VecFrames[i - 1]->cvframe->getPose().so3().inverse();
			}
			const IMUMeasure::Transformation &deltaPose = VecImuFactor[i]->deltaPose;
			const imuFactor::FacJBias_t &facJac = VecImuFactor[i]->getJBias();



			VecFrames[i - 1]->spbs.block<3, 1>(0, 0) = (scale * p2 - s_g(0, 0) * p1
			                                            + 0.5 * R_WI.matrix() * g_I_hat * G * dt12 * dt12 * delta_theta
			                                            - (R1_wb * (deltaPose.translation()
			                                             + facJac.block<3, 3>(9, 0) * s_dxy_ba.block<3, 1>(3, 0)))
			                                            - (R1_wc.matrix() - R2_wc.matrix()) * cP_B
			                                            - 0.5 * (imuParam->g) * dt12 * dt12) / dt12;

			//std::cout << "speed = \n" << VecFrames[i - 1]->spbs.block<3, 1>(0, 0) << std::endl;
		}
	}

	return true;

}
