#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Windows.h>
#include <openvr.h>
#include <vector>
#include <deque>

#include "Protocol.h"
#include "TrackingQuality.h"
#include "GuardianDrift.h"
#include "CalibrationCalc.h"
#include "IPCClient.h"


enum class CalibrationState
{
	None,
	Begin,
	Rotation,
	Translation,
	Editing,
	Continuous,
	ContinuousStandby,
};

struct StandbyDevice {
	std::string trackingSystem;
	std::string model, serial;
};

struct CalibrationContext
{
	CalibrationState state = CalibrationState::None;
	int32_t referenceID = -1, targetID = -1;

	static const size_t MAX_CONTROLLERS = 8;
	int32_t controllerIDs[MAX_CONTROLLERS];

	StandbyDevice targetStandby, referenceStandby;

	Eigen::Vector3d calibratedRotation;
	Eigen::Vector3d calibratedTranslation;
	double calibratedScale;

	std::string referenceTrackingSystem;
	std::string targetTrackingSystem;

	bool enabled = false;
	bool validProfile = false;
	bool clearOnLog = false;
	bool quashTargetInContinuous = false;
	double timeLastTick = 0, timeLastScan = 0, timeLastAssign = 0, timeLastPeriodicLog = 0;
	bool ignoreOutliers = false;
	double wantedUpdateInterval = 1.0;
	float jitterThreshold = 3.0f;

	bool requireTriggerPressToApply = false;
	bool wasWaitingForTriggers = false;
	bool hasAppliedCalibrationResult = false;

	float continuousCalibrationThreshold;
	float maxRelativeErrorThreshold = 0.005f;
	Eigen::Vector3d continuousCalibrationOffset;

	static constexpr size_t ContinuousSampleCount = 40;
	float continuousSpikeThresholdM = 0.06f;
	int continuousFrozenFrameThreshold = 6;
	bool pauseOnReferenceJitter = true;
	bool slamReference = false;
	bool applyHeadModelToReference = true;
	bool rejectYawDriftPoses = true;
	bool trustTargetYaw = true;
	bool compensatePoseTimeOffset = true;
	float maxReferencePoseTimeOffset = 0.04f;
	float maxPoseTimeSkew = 0.05f;

	float guardianDriftTransThresholdM = 0.02f;
	float guardianDriftYawThresholdRad = 3.0f * static_cast<float>(EIGEN_PI / 180.0);
	int guardianDriftConfirmChecks = 2;
	int guardianDriftCooldownFrames = 20;

	DeviceTrackingState referenceTracking;
	LARGE_INTEGER devicePoseSampleTime[vr::k_unMaxTrackedDeviceCount];

	protocol::AlignmentSpeedParams alignmentSpeedParams;
	bool enableStaticRecalibration;
	bool lockRelativePosition = false;

	Eigen::AffineCompact3d refToTargetPose = Eigen::AffineCompact3d::Identity();
	bool relativePosCalibrated = false;

	enum Speed
	{
		FAST = 0,
		SLOW = 1,
		VERY_SLOW = 2
	};
	Speed calibrationSpeed = FAST;

	vr::DriverPose_t devicePoses[vr::k_unMaxTrackedDeviceCount];

	CalibrationContext() {
		calibratedScale = 1.0;
		memset(devicePoses, 0, sizeof(devicePoses));
		memset(devicePoseSampleTime, 0, sizeof(devicePoseSampleTime));
		ResetConfig();
	}

	void ResetConfig() {
		alignmentSpeedParams.thr_rot_tiny = 0.49f * (EIGEN_PI / 180.0f);
		alignmentSpeedParams.thr_rot_small = 0.5f * (EIGEN_PI / 180.0f);
		alignmentSpeedParams.thr_rot_large = 5.0f * (EIGEN_PI / 180.0f);

		alignmentSpeedParams.thr_trans_tiny = 0.98f / 1000.0; // mm
		alignmentSpeedParams.thr_trans_small = 1.0f / 1000.0; // mm
		alignmentSpeedParams.thr_trans_large = 20.0f / 1000.0; // mm

		alignmentSpeedParams.align_speed_tiny = 1.0f;
		alignmentSpeedParams.align_speed_small = 1.0f;
		alignmentSpeedParams.align_speed_large = 2.0f;

		continuousCalibrationThreshold = 1.5f;
		maxRelativeErrorThreshold = 0.005f;
		jitterThreshold = 0.08f;

		continuousCalibrationOffset = Eigen::Vector3d::Zero();
		continuousSpikeThresholdM = 0.06f;
		continuousFrozenFrameThreshold = 6;
		pauseOnReferenceJitter = true;

		enableStaticRecalibration = true;
		slamReference = false;
		applyHeadModelToReference = true;
		rejectYawDriftPoses = true;
		trustTargetYaw = true;
		compensatePoseTimeOffset = true;
		maxReferencePoseTimeOffset = 0.04f;
		maxPoseTimeSkew = 0.05f;
	}

	float EffectiveSpikeThresholdM(const CalibrationCalc& calc) const {
		float threshold = continuousSpikeThresholdM;
		if (slamReference && calc.SampleCount() >= 3) {
			const double jitter = calc.ReferenceJitter();
			const double scale = 1.0 + std::min(jitter / 0.05, 2.5);
			threshold = static_cast<float>(continuousSpikeThresholdM * scale);
		}
		return threshold;
	}

	// Tuned for Quest/VD/SLAM reference + lighthouse head tracker (see audit logs).
	void ApplySlamReferencePreset() {
		enableStaticRecalibration = true;
		pauseOnReferenceJitter = false;
		continuousSpikeThresholdM = 0.05f;
		continuousFrozenFrameThreshold = 5;
		jitterThreshold = 0.15f;
		maxRelativeErrorThreshold = 0.008f;
		applyHeadModelToReference = true;
		rejectYawDriftPoses = true;
		trustTargetYaw = true;
		compensatePoseTimeOffset = true;
		maxReferencePoseTimeOffset = 0.04f;
		maxPoseTimeSkew = 0.05f;
		guardianDriftTransThresholdM = 0.035f;
		guardianDriftYawThresholdRad = 5.0f * static_cast<float>(EIGEN_PI / 180.0);
		guardianDriftConfirmChecks = 3;
		guardianDriftCooldownFrames = 60;
		calibrationSpeed = SLOW;
	}

	struct Chaperone
	{
		bool valid = false;
		bool autoApply = true;
		std::vector<vr::HmdQuad_t> geometry;
		vr::HmdMatrix34_t standingCenter = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
		};
		vr::HmdVector2_t playSpaceSize = { 0.0f, 0.0f };
	} chaperone;

	bool autoRecalOnGuardianDrift = true;
	GuardianDriftState guardianDrift;

	void ClearLogOnMessage() {
		clearOnLog = true;
	}

	void Clear()
	{
		chaperone.geometry.clear();
		chaperone.standingCenter = vr::HmdMatrix34_t();
		chaperone.playSpaceSize = vr::HmdVector2_t();
		chaperone.valid = false;

		calibratedRotation = Eigen::Vector3d();
		calibratedTranslation = Eigen::Vector3d();
		calibratedScale = 1.0;
		referenceTrackingSystem = "";
		targetTrackingSystem = "";
		enabled = false;
		validProfile = false;
		refToTargetPose = Eigen::AffineCompact3d::Identity();
		relativePosCalibrated = true;
	}

	size_t SampleCount()
	{
		if (state == CalibrationState::Continuous || state == CalibrationState::ContinuousStandby) {
			return ContinuousSampleCount;
		}

		switch (calibrationSpeed)
		{
		case FAST:
			return 100;
		case SLOW:
			return 250;
		case VERY_SLOW:
			return 500;
		}
		return 100;
	}

	struct Message
	{
		enum Type
		{
			String,
			Progress
		} type = String;

		Message(Type type) : type(type), progress(0), target(0) { }

		std::string str;
		int progress, target;
	};

	std::deque<Message> messages;

	void Log(const std::string &msg)
	{
		if (clearOnLog) {
			messages.clear();
			clearOnLog = false;
		}

		if (messages.empty() || messages.back().type == Message::Progress)
			messages.push_back(Message(Message::String));

		OutputDebugStringA(msg.c_str());

		messages.back().str += msg;
		std::cerr << msg;

		while (messages.size() > 15) messages.pop_front();
	}

	void Progress(int current, int target)
	{
		if (messages.empty() || messages.back().type == Message::String)
			messages.push_back(Message(Message::Progress));

		messages.back().progress = current;
		messages.back().target = target;
	}

	bool TargetPoseIsValidSimple() const {
		return targetID >= 0 && targetID <= vr::k_unMaxTrackedDeviceCount
			&& devicePoses[targetID].poseIsValid && devicePoses[targetID].result == vr::ETrackingResult::TrackingResult_Running_OK;
	}

	bool ReferencePoseIsValidSimple() const {
		return referenceID >= 0 && referenceID <= vr::k_unMaxTrackedDeviceCount
			&& devicePoses[referenceID].poseIsValid && devicePoses[referenceID].result == vr::ETrackingResult::TrackingResult_Running_OK;
	}
};

extern CalibrationContext CalCtx;
extern IPCClient Driver;

bool CollectSampleForChain(
	CalibrationContext& ctx,
	CalibrationCalc& calc,
	int referenceID,
	int targetID,
	const Eigen::Vector3d& targetOffset,
	bool slamReference,
	bool lockRelativePosition
);
struct CalibrationChain;
void ApplyChainCalibration(const CalibrationChain& chain, bool lerp);

void InitCalibrator();
void CalibrationTick(double time);
void StartCalibration();
void StartContinuousCalibration();
void EndContinuousCalibration();
void LoadChaperoneBounds();
void ApplyChaperoneBounds();

void PushCalibrationApplyTime();
void ShowCalibrationDebug(int r, int c);
void DebugApplyRandomOffset();