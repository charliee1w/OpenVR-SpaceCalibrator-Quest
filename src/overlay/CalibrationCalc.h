#pragma once

#include <Eigen/Dense>
#include <openvr.h>
#include <vector>
#include <deque>
#include <iostream>

struct Pose
{
	Eigen::Matrix3d rot;
	Eigen::Vector3d trans;

	Pose() { }
	Pose(const Eigen::AffineCompact3d& transform) {
		rot = transform.rotation();
		trans = transform.translation();
	}
	
	Pose(vr::HmdMatrix34_t hmdMatrix)
	{
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				rot(i, j) = hmdMatrix.m[i][j];
			}
		}
		trans = Eigen::Vector3d(hmdMatrix.m[0][3], hmdMatrix.m[1][3], hmdMatrix.m[2][3]);
	}
	Pose(vr::HmdQuaternion_t rot, const double *trans) {
		this->rot = Eigen::Matrix3d(Eigen::Quaterniond(rot.w, rot.x, rot.y, rot.z));
		this->trans = Eigen::Vector3d(trans[0], trans[1], trans[2]);
	}
	Pose(double x, double y, double z) : trans(Eigen::Vector3d(x, y, z)) { }

	Eigen::Matrix4d ToAffine() const {
		Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();

		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				matrix(i, j) = rot(i, j);
			}
			matrix(i, 3) = trans(i);
		}

		return matrix;
	}
};

struct Sample
{
	Pose ref, target;
	bool valid;
	double timestamp;
	Sample() : valid(false), timestamp(0) { }
	Sample(Pose ref, Pose target, double timestamp) : valid(true), ref(ref), target(target), timestamp(timestamp){ }
};

class CalibrationCalc {
public:
	static const double AxisVarianceThreshold;

	bool enableStaticRecalibration;
	bool lockRelativePosition = false;
	
	const Eigen::AffineCompact3d Transformation() const 
	{
		return m_estimatedTransformation;
	}

	const Eigen::Vector3d EulerRotation() const {
		auto rot = m_estimatedTransformation.rotation();
		return rot.eulerAngles(2, 1, 0) * 180.0 / EIGEN_PI;
	}

	bool isValid() const {
		return m_isValid;
	}
	
	const Eigen::AffineCompact3d RelativeTransformation() const 
	{
		return m_refToTargetPose;
	}

	bool isRelativeTransformationCalibrated() const
	{
		return m_relativePosCalibrated;
	}

	void setRelativeTransformation(const Eigen::AffineCompact3d transform, bool calibrated)
	{
		m_refToTargetPose = transform;
		m_relativePosCalibrated = calibrated;
	}

	// Seed the internal estimated transform from saved profile so enabling cont cal
	// doesn't cause an immediate jump/offset from re-computing based on current live samples.
	void SeedEstimatedTransformation(const Eigen::AffineCompact3d& est) {
		m_estimatedTransformation = est;
		m_isValid = true;
	}

	void PushSample(const Sample& sample);
	void Clear();
	void ResetContinuousGuards();

	// Continuous-session entry: clear sample window only; keep one-shot profile inputs intact.
	void BeginContinuousSession(
		const Eigen::AffineCompact3d& refToTargetPose,
		bool relativePosCalibrated,
		bool lockRelativePosition,
		const Eigen::AffineCompact3d* seedFromSavedProfile = nullptr);

	static Eigen::AffineCompact3d AffineFromSavedProfile(
		const Eigen::Vector3d& translationCm,
		const Eigen::Vector3d& eulerDeg);

	double ReferenceJitter() const;
	double TargetJitter() const;

	Eigen::Vector3d ComputeRigidRefToTargetOffset(const Pose& ref, const Pose& target) const;
	bool ShouldRejectContinuousSample(const Eigen::Vector3d& instantOffset, float spikeThresholdM) const;
	void NoteContinuousSampleOffset(const Eigen::Vector3d& instantOffset);

	Pose EffectiveReferencePose(const Sample& sample) const;
	Sample EffectiveSample(const Sample& sample) const;

	bool ComputeOneshot(const bool ignoreOutliers);
	bool ComputeIncremental(bool &lerp, double threshold, double relPoseMaxError, const bool ignoreOutliers);
	void RecordLiveMetrics(const Pose& refPose, const Pose& targetPose);

	size_t SampleCount() const {
		return m_samples.size();
	}

	void ShiftSample() {
		if (!m_samples.empty()) m_samples.pop_front();
	}

	void PruneRecentSamples(size_t count);

	CalibrationCalc() : m_isValid(false), m_calcCycle(0), enableStaticRecalibration(false) {}

	// Debug fields
	Eigen::Vector3d m_posOffset;
	double m_axisVariance = 0.0;
	long m_calcCycle;

private:
	bool m_isValid;
	Eigen::AffineCompact3d m_estimatedTransformation;
	bool m_relativePosCalibrated = false;

	/*
	 * This affine transform estimates the pose of the target within the reference device's local pose space.
	 * That is to say, it's given by transforming the target world pose by the inverse reference pose.
	 */
	Eigen::AffineCompact3d m_refToTargetPose = Eigen::AffineCompact3d::Identity();

	std::deque<Sample> m_samples;

	bool m_hasLastInstantOffset = false;
	Eigen::Vector3d m_lastInstantOffset = Eigen::Vector3d::Zero();
	bool m_hasLastRawPosOffset = false;
	Eigen::Vector3d m_lastRawPosOffset = Eigen::Vector3d::Zero();
	int m_frozenOffsetFrames = 0;
	int m_staticRelPoseConfirmFrames = 0;

	bool m_hasSavedProfileSeed = false;
	Eigen::AffineCompact3d m_savedProfileSeed = Eigen::AffineCompact3d::Identity();
	bool m_hasLastTrackedAppliedOffset = false;
	Eigen::Vector3d m_lastTrackedAppliedOffset = Eigen::Vector3d::Zero();
	int m_stuckAppliedOffsetFrames = 0;
	int m_divergedConditionFrames = 0;
	double m_lastDivergedRecoveryTime = 0.0;

	bool ConfirmStaticRelPoseApply(bool eligible, bool allowImmediate);
	void TrackAppliedOffsetStall(const Eigen::Vector3d& priorPosOffset);
	void AnnotateLockRelReject(const char* reason, double detailMm, bool driftRecovery);
	bool CalibrateByRelPoseRecent(size_t maxSamples, Eigen::AffineCompact3d& out) const;
	bool TryInstantRelCalibration(Eigen::AffineCompact3d& out, double& errorOut) const;
	bool WithinRecoveryJump(const Eigen::AffineCompact3d& candidate, double maxTransM, double maxRotRad) const;
	bool AttemptDivergedRecovery(double priorErrorM, Eigen::AffineCompact3d& out, double& outError);

	std::vector<bool> DetectOutliers() const;
	Eigen::Vector3d CalibrateRotation(const bool ignoreOutliers) const;
	Eigen::Vector3d CalibrateTranslation(const Eigen::Matrix3d &rotation) const;
	void CalibrateScaleOffset(const Eigen::Matrix3d &rotation, Eigen::Vector3d* out_scaleOffset, float* out_scaleFactor) const;

	Eigen::AffineCompact3d ComputeCalibration(const bool ignoreOutliers) const;

	double RetargetingErrorRMS(const Eigen::Vector3d& hmdToTargetPos, const Eigen::AffineCompact3d& calibration) const;
	Eigen::Vector3d ComputeRefToTargetOffset(const Eigen::AffineCompact3d& calibration) const;

	Eigen::Vector4d ComputeAxisVariance(const Eigen::AffineCompact3d& calibration) const;

	[[nodiscard]] bool ValidateCalibration(const Eigen::AffineCompact3d& calibration, double *errorOut = nullptr, Eigen::Vector3d* posOffsetV = nullptr) const;
	void ComputeInstantOffset();

	Eigen::AffineCompact3d EstimateRefToTargetPose(const Eigen::AffineCompact3d& calibration) const;
	bool CalibrateByRelPose(Eigen::AffineCompact3d &out) const;
};