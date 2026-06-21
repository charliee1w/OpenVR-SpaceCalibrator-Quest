#include "stdafx.h"
#include "Calibration.h"
#include "CalibrationMetrics.h"
#include "Configuration.h"
#include "IPCClient.h"
#include "CalibrationCalc.h"
#include "CalibrationChain.h"
#include "GuardianDrift.h"

#include "VRState.h"

#include <string>
#include <vector>
#include <iostream>

#include <Eigen/Dense>
#include <GLFW/glfw3.h>

inline vr::HmdQuaternion_t operator*(const vr::HmdQuaternion_t& lhs, const vr::HmdQuaternion_t& rhs) {
	return {
		(lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z),
		(lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y),
		(lhs.w * rhs.y) + (lhs.y * rhs.w) + (lhs.z * rhs.x) - (lhs.x * rhs.z),
		(lhs.w * rhs.z) + (lhs.z * rhs.w) + (lhs.x * rhs.y) - (lhs.y * rhs.x)
	};
}

CalibrationContext CalCtx;
IPCClient Driver;
static protocol::DriverPoseShmem shmem;

namespace {
	CalibrationCalc calibration;

	bool SafeDriverSend(const protocol::Request& req) {
		try {
			Driver.SendBlocking(req);
			return true;
		} catch (const std::exception& e) {
			CalCtx.Log(std::string("Driver IPC failed: ") + e.what() + "\n");
			return false;
		}
	}

	inline vr::HmdVector3d_t quaternionRotateVector(const vr::HmdQuaternion_t& quat, const double(&vector)[3]) {
		vr::HmdQuaternion_t vectorQuat = { 0.0, vector[0], vector[1] , vector[2] };
		vr::HmdQuaternion_t conjugate = { quat.w, -quat.x, -quat.y, -quat.z };
		auto rotatedVectorQuat = quat * vectorQuat * conjugate;
		return { rotatedVectorQuat.x, rotatedVectorQuat.y, rotatedVectorQuat.z };
	}

	inline Eigen::Matrix3d quaternionRotateMatrix(const vr::HmdQuaternion_t& quat) {
		return Eigen::Quaterniond(quat.w, quat.x, quat.y, quat.z).toRotationMatrix();
	}

	struct DSample
	{
		bool valid;
		Eigen::Vector3d ref, target;
	};

	bool StartsWith(const std::string& str, const std::string& prefix)
	{
		if (str.length() < prefix.length())
			return false;

		return str.compare(0, prefix.length(), prefix) == 0;
	}

	bool EndsWith(const std::string& str, const std::string& suffix)
	{
		if (str.length() < suffix.length())
			return false;

		return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
	}

	Eigen::Vector3d AxisFromRotationMatrix3(Eigen::Matrix3d rot)
	{
		return Eigen::Vector3d(rot(2, 1) - rot(1, 2), rot(0, 2) - rot(2, 0), rot(1, 0) - rot(0, 1));
	}

	double AngleFromRotationMatrix3(Eigen::Matrix3d rot)
	{
		return acos((rot(0, 0) + rot(1, 1) + rot(2, 2) - 1.0) / 2.0);
	}

	vr::HmdQuaternion_t VRRotationQuat(const Eigen::Quaterniond& rotQuat)
	{

		vr::HmdQuaternion_t vrRotQuat;
		vrRotQuat.x = rotQuat.coeffs()[0];
		vrRotQuat.y = rotQuat.coeffs()[1];
		vrRotQuat.z = rotQuat.coeffs()[2];
		vrRotQuat.w = rotQuat.coeffs()[3];
		return vrRotQuat;
	}
	
	vr::HmdQuaternion_t VRRotationQuat(Eigen::Vector3d eulerdeg)
	{
		auto euler = eulerdeg * EIGEN_PI / 180.0;

		Eigen::Quaterniond rotQuat =
			Eigen::AngleAxisd(euler(0), Eigen::Vector3d::UnitZ()) *
			Eigen::AngleAxisd(euler(1), Eigen::Vector3d::UnitY()) *
			Eigen::AngleAxisd(euler(2), Eigen::Vector3d::UnitX());

		return VRRotationQuat(rotQuat);
	}

	vr::HmdVector3d_t VRTranslationVec(Eigen::Vector3d transcm)
	{
		auto trans = transcm * 0.01;
		vr::HmdVector3d_t vrTrans;
		vrTrans.v[0] = trans[0];
		vrTrans.v[1] = trans[1];
		vrTrans.v[2] = trans[2];
		return vrTrans;
	}

	DSample DeltaRotationSamples(Sample s1, Sample s2)
	{
		// Difference in rotation between samples.
		auto dref = s1.ref.rot * s2.ref.rot.transpose();
		auto dtarget = s1.target.rot * s2.target.rot.transpose();

		// When stuck together, the two tracked objects rotate as a pair,
		// therefore their axes of rotation must be equal between any given pair of samples.
		DSample ds;
		ds.ref = AxisFromRotationMatrix3(dref);
		ds.target = AxisFromRotationMatrix3(dtarget);

		// Reject samples that were too close to each other.
		auto refA = AngleFromRotationMatrix3(dref);
		auto targetA = AngleFromRotationMatrix3(dtarget);
		ds.valid = refA > 0.4 && targetA > 0.4 && ds.ref.norm() > 0.01 && ds.target.norm() > 0.01;

		ds.ref.normalize();
		ds.target.normalize();
		return ds;
	}

	double PoseSampleAgeSeconds(const CalibrationContext& ctx, int deviceId) {
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		const auto& ts = ctx.devicePoseSampleTime[deviceId];
		if (ts.QuadPart == 0) {
			return 0.0;
		}
		return (now.QuadPart - ts.QuadPart) / (double)freq.QuadPart;
	}

	Pose ConvertPose(const vr::DriverPose_t &driverPose, bool applyHeadModel = false, bool compensateLatency = false) {
		Eigen::Quaterniond driverToWorldQ(
			driverPose.qWorldFromDriverRotation.w,
			driverPose.qWorldFromDriverRotation.x,
			driverPose.qWorldFromDriverRotation.y,
			driverPose.qWorldFromDriverRotation.z
		);
		Eigen::Vector3d driverToWorldV(
			driverPose.vecWorldFromDriverTranslation[0],
			driverPose.vecWorldFromDriverTranslation[1],
			driverPose.vecWorldFromDriverTranslation[2]
		);

		Eigen::Quaterniond deviceQ(
			driverPose.qRotation.w,
			driverPose.qRotation.x,
			driverPose.qRotation.y,
			driverPose.qRotation.z
		);
		Eigen::Vector3d devicePos(
			driverPose.vecPosition[0],
			driverPose.vecPosition[1],
			driverPose.vecPosition[2]
		);

		Eigen::Quaterniond bodyQ = deviceQ;
		Eigen::Vector3d bodyPos = devicePos;
		if (applyHeadModel || driverPose.shouldApplyHeadModel) {
			Eigen::Quaterniond headFromDriverQ(
				driverPose.qDriverFromHeadRotation.w,
				driverPose.qDriverFromHeadRotation.x,
				driverPose.qDriverFromHeadRotation.y,
				driverPose.qDriverFromHeadRotation.z
			);
			Eigen::Vector3d headFromDriverV(
				driverPose.vecDriverFromHeadTranslation[0],
				driverPose.vecDriverFromHeadTranslation[1],
				driverPose.vecDriverFromHeadTranslation[2]
			);
			bodyQ = deviceQ * headFromDriverQ;
			bodyPos = devicePos + deviceQ * headFromDriverV;
		}

		if (compensateLatency && std::abs(driverPose.poseTimeOffset) > 1e-5) {
			const double t = driverPose.poseTimeOffset;
			Eigen::Vector3d linearVel(
				driverPose.vecVelocity[0],
				driverPose.vecVelocity[1],
				driverPose.vecVelocity[2]
			);
			bodyPos += bodyQ * linearVel * t;

			Eigen::Vector3d angularVel(
				driverPose.vecAngularVelocity[0],
				driverPose.vecAngularVelocity[1],
				driverPose.vecAngularVelocity[2]
			);
			const double angle = angularVel.norm() * t;
			if (angle > 1e-6) {
				Eigen::AngleAxisd axisAngle(angle, angularVel.normalized());
				bodyQ = Eigen::Quaterniond(axisAngle) * bodyQ;
			}
		}

		Eigen::Quaterniond driverRot = driverToWorldQ * bodyQ;
		Eigen::Vector3d driverPos = driverToWorldV + driverToWorldQ * bodyPos;

		Eigen::AffineCompact3d xform = Eigen::Translation3d(driverPos) * driverRot;

		return Pose(xform);
	}

	bool PosePassesQualityGate(const vr::DriverPose_t& pose, const char* label, bool rejectYawDrift) {
		if (!pose.poseIsValid || pose.result != vr::ETrackingResult::TrackingResult_Running_OK) {
			char buf[128];
			snprintf(buf, sizeof buf, "%s pose rejected: invalid tracking (valid=%d result=%d)\n",
				label, pose.poseIsValid, pose.result);
			CalCtx.Log(buf);
			return false;
		}
		if (rejectYawDrift && pose.willDriftInYaw) {
			char buf[128];
			snprintf(buf, sizeof buf, "%s pose rejected: SLAM yaw drift warning\n", label);
			CalCtx.Log(buf);
			return false;
		}
		return true;
	}

	bool CollectSampleForChainImpl(
	CalibrationContext& ctx,
	CalibrationCalc& calc,
	int referenceID,
	int targetID,
	const Eigen::Vector3d& targetOffset,
	bool slamReference,
	bool lockRelativePosition)
{
	if (referenceID < 0 || targetID < 0
		|| referenceID >= vr::k_unMaxTrackedDeviceCount
		|| targetID >= vr::k_unMaxTrackedDeviceCount) {
		return false;
	}

	vr::DriverPose_t reference = ctx.devicePoses[referenceID];
	vr::DriverPose_t target = ctx.devicePoses[targetID];

	const bool rejectRefYawDrift = slamReference && ctx.rejectYawDriftPoses;
	bool ok = PosePassesQualityGate(reference, "Reference", rejectRefYawDrift)
		&& PosePassesQualityGate(target, "Target", false);
	if (!ok) {
		if (ctx.state == CalibrationState::Continuous) {
			calc.PruneRecentSamples(4);
			return false;
		}
		ctx.Log("Aborting calibration!\n");
		ctx.state = CalibrationState::None;
		return false;
	}

	if (ctx.state == CalibrationState::Continuous || ctx.state == CalibrationState::ContinuousStandby) {
		reference.vecPosition[0] += targetOffset.x();
		reference.vecPosition[1] += targetOffset.y();
		reference.vecPosition[2] += targetOffset.z();
	}

	if (slamReference && ctx.compensatePoseTimeOffset) {
		if (std::abs(reference.poseTimeOffset) > ctx.maxReferencePoseTimeOffset) {
			if (ctx.state == CalibrationState::Continuous) {
				return false;
			}
			ctx.Log("Reference pose rejected: IMU extrapolation latency\n");
			return false;
		}

		const double skew = std::abs(
			PoseSampleAgeSeconds(ctx, referenceID) - PoseSampleAgeSeconds(ctx, targetID)
		);
		if (skew > ctx.maxPoseTimeSkew) {
			if (ctx.state == CalibrationState::Continuous) {
				return false;
			}
			ctx.Log("Pose pair rejected: ref/target sample time skew\n");
			return false;
		}
	}

	const bool compensateLatency = slamReference && ctx.compensatePoseTimeOffset;
	Pose refPose = ConvertPose(reference, slamReference && ctx.applyHeadModelToReference, compensateLatency);
	const Pose targetPose = ConvertPose(target, false, false);

	if (slamReference && ctx.trustTargetYaw && calc.isRelativeTransformationCalibrated()) {
		refPose.rot = targetPose.rot * calc.RelativeTransformation().rotation().inverse();
	}

	if (ctx.state == CalibrationState::Continuous) {
		const Eigen::Vector3d instantOffset = calc.ComputeRigidRefToTargetOffset(refPose, targetPose);
		if (calc.ShouldRejectContinuousSample(instantOffset, ctx.EffectiveSpikeThresholdM(calc))) {
			return false;
		}
		calc.NoteContinuousSampleOffset(instantOffset);
	}

	calc.PushSample(Sample(refPose, targetPose, glfwGetTime()));
	(void)lockRelativePosition;
	return true;
	}

	void ApplyChainCalibrationImpl(const CalibrationChain& chain, bool lerp)
	{
	if (!chain.valid) {
		return;
	}

	CalibrationChain resolved = chain;
	AssignChainTargets(resolved);

	auto vrTrans = VRTranslationVec(resolved.calibratedTranslation);
	auto vrRot = VRRotationQuat(resolved.calibratedRotation);

	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
		auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
		if (deviceClass == vr::TrackedDeviceClass_Invalid || id == vr::k_unTrackedDeviceIndex_Hmd) {
			continue;
		}

		char buffer[vr::k_unMaxPropertyStringSize] = {};
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_TrackingSystemName_String, buffer, sizeof(buffer), &err);
		if (err != vr::TrackedProp_Success || std::string(buffer) != resolved.targetTrackingSystem) {
			continue;
		}

		protocol::Request req(protocol::RequestSetDeviceTransform);
		req.setDeviceTransform = { id, true, vrTrans, vrRot, resolved.calibratedScale };
		req.setDeviceTransform.lerp = lerp;
		req.setDeviceTransform.quash = CalCtx.state == CalibrationState::Continuous
			&& (int)id == resolved.targetID
			&& CalCtx.quashTargetInContinuous;
		SafeDriverSend(req);
	}
	}

	uint64_t GetTrackedDevicePresenceMask() {
		uint64_t mask = 0;
		for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
			if (vr::VRSystem()->GetTrackedDeviceClass(id) != vr::TrackedDeviceClass_Invalid) {
				mask |= (1ULL << id);
			}
		}
		return mask;
	}

	bool AssignTargets() {
		auto state = VRState::Load();

		const int resolvedReference = state.ResolveStandbyDevice(
			CalCtx.referenceStandby, CalCtx.referenceTrackingSystem);
		if (resolvedReference >= 0
			&& (CalCtx.referenceID < 0 || !VRState::IsDeviceConnected(CalCtx.referenceID))) {
			CalCtx.referenceID = resolvedReference;
		}

		const int resolvedTarget = state.ResolveStandbyDevice(
			CalCtx.targetStandby, CalCtx.targetTrackingSystem);
		if (resolvedTarget >= 0
			&& (CalCtx.targetID < 0 || !VRState::IsDeviceConnected(CalCtx.targetID))) {
			CalCtx.targetID = resolvedTarget;
		}

		for (int i = 0; i < CalCtx.MAX_CONTROLLERS; i++) {
			if (i < state.devices.size()
				&& state.devices[i].trackingSystem == CalCtx.targetTrackingSystem
				&& state.devices[i].deviceClass == vr::TrackedDeviceClass_Controller
				&& (state.devices[i].controllerRole == vr::TrackedControllerRole_LeftHand || state.devices[i].controllerRole == vr::TrackedControllerRole_RightHand))
			{
				CalCtx.controllerIDs[i] = state.devices[i].id;
			} else {
				CalCtx.controllerIDs[i] = -1;
			}
		}

		CalCtx.slamReference = VRState::IsSlamTrackingSystem(CalCtx.referenceTrackingSystem);

		return CalCtx.referenceID >= 0 && CalCtx.targetID >= 0;
	}

	bool CollectSample(const CalibrationContext& ctx)
	{
		return CollectSampleForChainImpl(
			const_cast<CalibrationContext&>(ctx),
			calibration,
			ctx.referenceID,
			ctx.targetID,
			ctx.continuousCalibrationOffset,
			ctx.slamReference,
			CalCtx.lockRelativePosition
		);
	}
}

bool CollectSampleForChain(
	CalibrationContext& ctx,
	CalibrationCalc& calc,
	int referenceID,
	int targetID,
	const Eigen::Vector3d& targetOffset,
	bool slamReference,
	bool lockRelativePosition)
{
	return CollectSampleForChainImpl(
		ctx, calc, referenceID, targetID, targetOffset, slamReference, lockRelativePosition);
}

void ApplyChainCalibration(const CalibrationChain& chain, bool lerp)
{
	ApplyChainCalibrationImpl(chain, lerp);
}

void InitCalibrator()
{
	Driver.Connect();
	shmem.Open(OPENVR_SPACECALIBRATOR_SHMEM_NAME);
}

void ResetAndDisableOffsets(uint32_t id)
{
	vr::HmdVector3d_t zeroV;
	zeroV.v[0] = zeroV.v[1] = zeroV.v[2] = 0;

	vr::HmdQuaternion_t zeroQ;
	zeroQ.x = 0; zeroQ.y = 0; zeroQ.z = 0; zeroQ.w = 1;

	protocol::Request req(protocol::RequestSetDeviceTransform);
	req.setDeviceTransform = { id, false, zeroV, zeroQ, 1.0 };
	SafeDriverSend(req);
}

static_assert(vr::k_unTrackedDeviceIndex_Hmd == 0, "HMD index expected to be 0");

void ScanAndApplyProfile(CalibrationContext &ctx)
{
	AssignTargets();
	SyncCalCtxToPrimaryChain();

	std::unique_ptr<char[]> buffer_array(new char [vr::k_unMaxPropertyStringSize]);
	char* buffer = buffer_array.get();
	bool applyEnabled = ctx.validProfile;
	bool hmdTrackingSystemOk = true;

	protocol::Request setParamsReq(protocol::RequestSetAlignmentSpeedParams);
	setParamsReq.setAlignmentSpeedParams = ctx.alignmentSpeedParams;
	if (!SafeDriverSend(setParamsReq)) {
		ctx.Log("Warning: alignment params IPC failed; continuing device apply\n");
	}

	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id)
	{
		auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
		if (deviceClass == vr::TrackedDeviceClass_Invalid)
			continue;

		/*if (deviceClass == vr::TrackedDeviceClass_HMD) // for debugging unexpected universe switches
		{
			vr::ETrackedPropertyError err = vr::TrackedProp_Success;
			auto universeId = vr::VRSystem()->GetUint64TrackedDeviceProperty(id, vr::Prop_CurrentUniverseId_Uint64, &err);
			printf("uid %d err %d\n", universeId, err);
			ResetAndDisableOffsets(id);
			continue;
		}*/

		if (!applyEnabled)
		{
			ResetAndDisableOffsets(id);
			continue;
		}

		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize, &err);

		if (err != vr::TrackedProp_Success)
		{
			ResetAndDisableOffsets(id);
			continue;
		}

		std::string trackingSystem(buffer);

		if (id == vr::k_unTrackedDeviceIndex_Hmd)
		{
			//auto p = ctx.devicePoses[id].mDeviceToAbsoluteTracking.m;
			//printf("HMD %d: %f %f %f\n", id, p[0][3], p[1][3], p[2][3]);

			// Check if the current HMD is a Pimax crystal
			if (trackingSystem == "aapvr") {
				// HMD is a Pimax HMD
				vr::HmdMatrix34_t eyeToHeadLeft = vr::VRSystem()->GetEyeToHeadTransform(vr::Eye_Left);
				// Crystal's projection matrix is constant 0s or 1s except for [0][3], which stores the IPD offset from the nose
				bool isCrystalHmd =
					eyeToHeadLeft.m[0][0] == 1 && eyeToHeadLeft.m[0][1] == 0 && eyeToHeadLeft.m[0][2] == 0 &&                     // IPD
					eyeToHeadLeft.m[1][0] == 0 && eyeToHeadLeft.m[1][1] == 1 && eyeToHeadLeft.m[1][2] == 0 && eyeToHeadLeft.m[1][3] == 0 &&
					eyeToHeadLeft.m[2][0] == 0 && eyeToHeadLeft.m[2][1] == 0 && eyeToHeadLeft.m[2][2] == 1 && eyeToHeadLeft.m[2][3] == 0;

				if (isCrystalHmd) {
					// Move it outside the aapvr system ; we treat aapvr as if it were lighthouse
					trackingSystem = "Pimax Crystal HMD";
				}
			}

			if (trackingSystem != ctx.referenceTrackingSystem)
			{
				hmdTrackingSystemOk = false;
			}

			ResetAndDisableOffsets(id);
			continue;
		}

		// Detect Pimax crystal controllers and separate them too
		if (deviceClass == vr::TrackedDeviceClass_Controller) {
			if (trackingSystem == "oculus") {
				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				std::string renderModel(buffer);
				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_ConnectedWirelessDongle_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				std::string connectedWirelessDongle(buffer);

				// Check if the controller claims its an oculus controller but also pimax
				if (renderModel.find("{aapvr}") != std::string::npos &&
					renderModel.find("crystal") != std::string::npos &&
					connectedWirelessDongle.find("lighthouse") != std::string::npos) {
					trackingSystem = "Pimax Crystal Controllers";
				}
			}
		}

		EnsureDefaultChain();
		const int chainIdx = FindChainIndexForTargetSystem(trackingSystem);
		if (chainIdx < 0) {
			ResetAndDisableOffsets(id);
			continue;
		}

		const CalibrationChain& chain = CalChains[chainIdx];
		protocol::Request req(protocol::RequestSetDeviceTransform);
		req.setDeviceTransform = {
			id,
			true,
			VRTranslationVec(chain.calibratedTranslation),
			VRRotationQuat(chain.calibratedRotation),
			chain.calibratedScale
		};
		req.setDeviceTransform.lerp = CalCtx.state == CalibrationState::Continuous;
		req.setDeviceTransform.quash = CalCtx.state == CalibrationState::Continuous
			&& id == (uint32_t)CalCtx.targetID
			&& CalCtx.quashTargetInContinuous;

		SafeDriverSend(req);
	}

	ctx.enabled = applyEnabled && hmdTrackingSystemOk;

	if (ctx.enabled && ctx.chaperone.valid && ctx.chaperone.autoApply)
	{
		uint32_t quadCount = 0;
		vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(nullptr, &quadCount);

		// Heuristic: when SteamVR resets to a blank-ish chaperone, it uses empty geometry,
		// but manual adjustments (e.g. via a play space mover) will not touch geometry.
		if (quadCount != ctx.chaperone.geometry.size())
		{
			ApplyChaperoneBounds();
		}
	}
}

namespace {
	bool ShouldApplySavedProfile(const CalibrationContext& ctx) {
		return ctx.validProfile && (
			ctx.state == CalibrationState::None
			|| ctx.state == CalibrationState::Continuous
			|| ctx.state == CalibrationState::ContinuousStandby
			|| ctx.state == CalibrationState::Editing
		);
	}

	void MaybeScanAndApplySavedProfile(CalibrationContext& ctx, double time) {
		if (!ShouldApplySavedProfile(ctx)) {
			return;
		}

		static uint64_t lastPresenceMask = 0;
		const uint64_t presenceMask = GetTrackedDevicePresenceMask();
		const bool devicesChanged = presenceMask != lastPresenceMask;
		lastPresenceMask = presenceMask;

		// During continuous cal, only rescan when trackers connect — not every second (64 IPC calls).
		if (ctx.state == CalibrationState::Continuous) {
			if (!devicesChanged) {
				return;
			}
		} else {
			const double scanInterval = (ctx.state == CalibrationState::Editing) ? 0.1 : 1.0;
			if (!devicesChanged && (time - ctx.timeLastScan) < scanInterval) {
				return;
			}
		}

		ScanAndApplyProfile(ctx);
		ctx.timeLastScan = time;
	}
}

void StartCalibration() {
	CalCtx.hasAppliedCalibrationResult = false;
	AssignTargets();
	CalCtx.state = CalibrationState::Begin;
	CalCtx.wantedUpdateInterval = 0.05;
	CalCtx.messages.clear();
	calibration.Clear();
	Metrics::WriteLogAnnotation("StartCalibration");
}

void StartContinuousCalibration() {
	Metrics::EnableSessionLogging();
	CalCtx.timeLastPeriodicLog = 0;
	CalCtx.hasAppliedCalibrationResult = false;
	AssignTargets();
	if (CalCtx.slamReference) {
		CalCtx.ApplySlamReferencePreset();
		CalCtx.Log("SLAM reference detected: Quest/inside-out preset applied\n");
		Metrics::WriteLogAnnotation("SlamReferencePreset");
	}
	StartCalibration();
	CalCtx.state = CalibrationState::Continuous;
	CalCtx.wantedUpdateInterval = 0.05;
	CalCtx.enableStaticRecalibration = true;
	calibration.enableStaticRecalibration = true;
	calibration.ResetContinuousGuards();
	calibration.setRelativeTransformation(CalCtx.refToTargetPose, CalCtx.relativePosCalibrated);
	calibration.lockRelativePosition = CalCtx.lockRelativePosition;
	if (CalCtx.validProfile) {
		calibration.SeedFromProfile(CalCtx.calibratedRotation, CalCtx.calibratedTranslation);
	}
	if (CalCtx.lockRelativePosition) {
		CalCtx.Log("Relative position locked");
	}
	else {
		CalCtx.Log("Collecting initial samples...");
	}
	StartContinuousChains();
	if (CalCtx.validProfile) {
		// Match upstream: apply hide-tracker (quash) immediately when cont cal starts,
		// not only after the first incremental cal update.
		ScanAndApplyProfile(CalCtx);
	}
	Metrics::WriteLogAnnotation("StartContinuousCalibration");
}

void EndContinuousCalibration() {
	SyncPrimaryChainToCalCtx();
	EndContinuousChains();
	CalCtx.state = CalibrationState::None;
	CalCtx.wantedUpdateInterval = 1.0;
	AssignTargets();
	if (CalCtx.validProfile) {
		ScanAndApplyProfile(CalCtx);
	}
	SyncCalCtxToPrimaryChain();
	SaveProfile(CalCtx);
	Metrics::WriteLogAnnotation("EndContinuousCalibration");
}

void CalibrationTick(double time)
{
	if (!vr::VRSystem())
		return;

	try {
	auto &ctx = CalCtx;
	if ((time - ctx.timeLastTick) < 0.05)
		return;

	if (ctx.state == CalibrationState::Continuous || ctx.state == CalibrationState::ContinuousStandby) {
		ctx.ClearLogOnMessage();

		if (CalCtx.requireTriggerPressToApply && (time - ctx.timeLastAssign) > 10) {
			// rescan devices every 10 seconds or so if we are using controller data
			ctx.timeLastAssign = time;
			AssignTargets();
		}
	}

	ctx.timeLastTick = time;

	// Apply saved cal to newly connected trackers before tracking-quality gates.
	// contcal13 stopped calling ScanAndApplyProfile during continuous cal, which broke
	// powering trackers on after setup (upstream re-applied every tick).
	MaybeScanAndApplySavedProfile(ctx, time);

	shmem.ReadNewPoses([&](const protocol::DriverPoseShmem::AugmentedPose& augmented_pose) {
		if (augmented_pose.deviceId >= 0 && augmented_pose.deviceId < vr::k_unMaxTrackedDeviceCount) {
			ctx.devicePoses[augmented_pose.deviceId] = augmented_pose.pose;
			ctx.devicePoseSampleTime[augmented_pose.deviceId] = augmented_pose.sample_time;
		}
	});

	const int trackingDeviceId = (ctx.referenceID >= 0) ? ctx.referenceID : vr::k_unTrackedDeviceIndex_Hmd;
	const auto& trackingPose = ctx.devicePoses[trackingDeviceId];
	const auto priorQuality = ctx.referenceTracking.quality;
	ctx.referenceTracking.quality = EvaluateDeviceTracking(
		trackingPose,
		ctx.referenceTracking,
		ctx.slamReference,
		ctx.maxReferencePoseTimeOffset,
		3
	);

	if (ShouldSkipCalibrationTick(ctx.referenceTracking.quality)) {
		if (ctx.referenceTracking.quality != priorQuality) {
			if (ctx.referenceTracking.quality == TrackingQuality::Frozen) {
				ctx.Log("Reference tracking frozen, skipping tick\n");
			} else if (ctx.referenceTracking.quality == TrackingQuality::Invalid) {
				ctx.Log("Reference tracking invalid, skipping tick\n");
			}
		}
		if (ctx.state == CalibrationState::Continuous) {
			calibration.PruneRecentSamples(3);
		}
		return;
	}

	if (ctx.state == CalibrationState::Continuous && ctx.slamReference
		&& ctx.referenceTracking.quality == TrackingQuality::Degraded) {
		calibration.PruneRecentSamples(3);
		return;
	}

	if (ctx.autoRecalOnGuardianDrift && ctx.state == CalibrationState::Continuous && ctx.slamReference) {
		if (HandleGuardianDrift(ctx, ctx.guardianDrift, time, calibration)) {
			for (auto& chain : CalChains) {
				if (chain.continuousActive) {
					chain.calibration.Clear();
					chain.calibration.ResetContinuousGuards();
				}
			}
		}
	}

	if (ctx.state == CalibrationState::ContinuousStandby) {
		if (AssignTargets()) {
			StartContinuousCalibration();
		}
		else {
			ctx.wantedUpdateInterval = 0.5;
			ctx.Log("Waiting for devices...\n");
			return;
		}
	}

	if (ctx.state == CalibrationState::None) {
		ctx.wantedUpdateInterval = 1.0;
		return;
	}

	if (ctx.state == CalibrationState::Editing)
	{
		ctx.wantedUpdateInterval = 0.1;
		return;
	}

	bool ok = true;

	if (ctx.referenceID == -1 || ctx.referenceID >= vr::k_unMaxTrackedDeviceCount) {
		CalCtx.Log("Missing reference device\n");
		ok = false;
	}
	if (ctx.targetID == -1 || ctx.targetID >= vr::k_unMaxTrackedDeviceCount)
	{
		CalCtx.Log("Missing target device\n");
		ok = false;
	}

	if (ctx.state == CalibrationState::Begin)
	{

		char referenceSerial[256], targetSerial[256];
		referenceSerial[0] = targetSerial[0] = 0;
		vr::VRSystem()->GetStringTrackedDeviceProperty(ctx.referenceID, vr::Prop_SerialNumber_String, referenceSerial, 256);
		vr::VRSystem()->GetStringTrackedDeviceProperty(ctx.targetID, vr::Prop_SerialNumber_String, targetSerial, 256);

		char buf[256];
		snprintf(buf, sizeof buf, "Reference device ID: %d, serial: %s\n", ctx.referenceID, referenceSerial);
		CalCtx.Log(buf);
		snprintf(buf, sizeof buf, "Target device ID: %d, serial %s\n", ctx.targetID, targetSerial);
		CalCtx.Log(buf);

		ScanAndApplyProfile(ctx);

		Metrics::jitterRef.Push(calibration.ReferenceJitter());
		Metrics::jitterTarget.Push(calibration.TargetJitter());

		if (!CalCtx.ReferencePoseIsValidSimple())
		{
			CalCtx.Log("Reference device is not tracking\n"); ok = false;
		}

		if (!CalCtx.TargetPoseIsValidSimple())
		{
			CalCtx.Log("Target device is not tracking\n"); ok = false;
		}
		
		// SLAM HMD references have ~10cm baseline jitter; only gate on target (lighthouse tracker).
		if (!ctx.slamReference && calibration.ReferenceJitter() > ctx.jitterThreshold) {
			CalCtx.Log("Reference device is not tracking\n"); ok = false;
		}
		if (calibration.TargetJitter() > ctx.jitterThreshold) {
			CalCtx.Log("Target device is not tracking\n"); ok = false;
		}

		if (ok) {
			//ResetAndDisableOffsets(ctx.targetID);
			ctx.state = CalibrationState::Rotation;
			ctx.wantedUpdateInterval = 0.05;

			CalCtx.Log("Starting calibration...\n");
			return;
		}
	}

	if (!ok)
	{
		if (ctx.state != CalibrationState::Continuous) {
			ctx.state = CalibrationState::None;

			CalCtx.Log("Aborting calibration!\n");
		}
		return;
	}

	if (ShouldRejectCalibrationSample(ctx.referenceTracking.quality)) {
		if (ctx.state == CalibrationState::Continuous) {
			return;
		}
	}

	if (!CollectSample(ctx))
	{
		return;
	}

	CalCtx.Progress((int) calibration.SampleCount(), (int)CalCtx.SampleCount());

	if (calibration.SampleCount() < CalCtx.SampleCount()) return;
	while (calibration.SampleCount() > CalCtx.SampleCount()) calibration.ShiftSample();

	if (CalCtx.state == CalibrationState::Continuous && CalCtx.pauseOnReferenceJitter && calibration.SampleCount() >= 3) {
		const double refJitter = calibration.ReferenceJitter();
		const double targetJitter = calibration.TargetJitter();
		Metrics::jitterRef.Push(refJitter);
		Metrics::jitterTarget.Push(targetJitter);

		if (refJitter > CalCtx.jitterThreshold) {
			CalCtx.Log("Paused continuous cal: high reference jitter\n");
			return;
		}
	}

	if (CalCtx.state == CalibrationState::Continuous && CalCtx.requireTriggerPressToApply && CalCtx.hasAppliedCalibrationResult) {
		bool triggerPressed = true;
		vr::VRControllerState_t state;
		for (int i = 0; i < CalCtx.MAX_CONTROLLERS; i++) {
			if (CalCtx.controllerIDs[i] >= 0) {
				vr::VRSystem()->GetControllerState(CalCtx.controllerIDs[i], &state, sizeof(state));
				triggerPressed &= state.rAxis[vr::k_eControllerAxis_TrackPad /* matches trigger on Index controllers?? */].x > 0.75f
					|| state.rAxis[vr::k_eControllerAxis_Trigger].x > 0.75f;
				//printf("Controller %d tracpad: %f\n", i, state.rAxis[vr::k_eControllerAxis_TrackPad].x);
				//printf("Controller %d trigger: %f\n", i, state.rAxis[vr::k_eControllerAxis_Trigger].x);
				if (!triggerPressed) {
					break;
				}
			}
		}

		if (!triggerPressed) {
			CalCtx.Log("Waiting for trigger press...\n");
			CalCtx.wasWaitingForTriggers = true;
			return;
		}

		if (CalCtx.wasWaitingForTriggers) {
			CalCtx.Log("Triggers pressed, continuing calibration...\n");
			CalCtx.wasWaitingForTriggers = false;
		}
	}

	LARGE_INTEGER start_time;
	QueryPerformanceCounter(&start_time);
		
	bool lerp = false;
	bool calUpdated = false;

	if (CalCtx.state == CalibrationState::Continuous) {
		CalCtx.wantedUpdateInterval = 0.05;
		CalCtx.messages.clear();
		calibration.enableStaticRecalibration = CalCtx.enableStaticRecalibration;
		calibration.lockRelativePosition = CalCtx.lockRelativePosition;
		calUpdated = calibration.ComputeIncremental(
			lerp,
			CalCtx.continuousCalibrationThreshold,
			CalCtx.maxRelativeErrorThreshold,
			CalCtx.ignoreOutliers
		);
	}
	else {
		calibration.enableStaticRecalibration = false;
		calibration.ComputeOneshot(CalCtx.ignoreOutliers);
		calUpdated = calibration.isValid();
	}

	if (CalCtx.state == CalibrationState::Continuous) {
		if (calUpdated && calibration.isValid()) {
			ctx.calibratedRotation = calibration.EulerRotation();
			ctx.calibratedTranslation = calibration.Transformation().translation() * 100.0;
			ctx.refToTargetPose = calibration.RelativeTransformation();
			ctx.relativePosCalibrated = calibration.isRelativeTransformationCalibrated();
			ctx.validProfile = true;
			SyncCalCtxToPrimaryChain();
			ApplyChainCalibration(CalChains[0], lerp);
			MaybeSaveProfile(ctx, time);
			CalCtx.hasAppliedCalibrationResult = true;
		}
	}
	else if (calibration.isValid()) {
		ctx.calibratedRotation = calibration.EulerRotation();
		ctx.calibratedTranslation = calibration.Transformation().translation() * 100.0;
		ctx.refToTargetPose = calibration.RelativeTransformation();
		ctx.relativePosCalibrated = calibration.isRelativeTransformationCalibrated();
		ctx.validProfile = true;
		SaveProfile(ctx);

		ScanAndApplyProfile(ctx);

		CalCtx.hasAppliedCalibrationResult = true;
		CalCtx.Log("Finished calibration, profile saved\n");
	} else {
		CalCtx.Log("Calibration failed.\n");
	}

	LARGE_INTEGER end_time;
	QueryPerformanceCounter(&end_time);
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	double duration = (end_time.QuadPart - start_time.QuadPart) / (double)freq.QuadPart;
	Metrics::computationTime.Push(duration * 1000.0);

	if (CalCtx.state != CalibrationState::Continuous) {
		Metrics::WriteLogEntry();
	}
		
	if (CalCtx.state != CalibrationState::Continuous) {
		ctx.state = CalibrationState::None;
		calibration.Clear();
	}
	else {
		size_t drop_samples = CalCtx.SampleCount() / 10;
		for (int i = 0; i < drop_samples; i++) {
			calibration.ShiftSample();
		}

		SyncCalCtxToPrimaryChain();
		for (size_t i = 1; i < CalChains.size(); i++) {
			if (CalChains[i].continuousActive) {
				ProcessContinuousChain(CalChains[i], ctx, time);
			}
		}
	}

	if (ctx.state == CalibrationState::Continuous
		&& ctx.referenceID >= 0 && ctx.targetID >= 0) {
		if (ctx.timeLastPeriodicLog == 0) {
			ctx.timeLastPeriodicLog = time;
		}
		else if ((time - ctx.timeLastPeriodicLog) >= 1.0) {
			ctx.timeLastPeriodicLog = time;

			const vr::DriverPose_t& reference = ctx.devicePoses[ctx.referenceID];
			const vr::DriverPose_t& target = ctx.devicePoses[ctx.targetID];
			if (reference.poseIsValid && target.poseIsValid) {
				const bool compensateLatency = ctx.slamReference && ctx.compensatePoseTimeOffset;
				Pose refPose = ConvertPose(
					reference,
					ctx.slamReference && ctx.applyHeadModelToReference,
					compensateLatency
				);
				Pose targetPose = ConvertPose(target, false, false);

				if (ctx.slamReference && ctx.trustTargetYaw && calibration.isRelativeTransformationCalibrated()) {
					refPose.rot = targetPose.rot * calibration.RelativeTransformation().rotation().inverse();
				}

				calibration.RecordLiveMetrics(refPose, targetPose);
				Metrics::WriteLogEntry();
			}
		}
	}
	} catch (const std::exception& e) {
		CalCtx.Log(std::string("Calibration tick error: ") + e.what() + "\n");
	}
}

void LoadChaperoneBounds()
{
	vr::VRChaperoneSetup()->RevertWorkingCopy();

	uint32_t quadCount = 0;
	vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(nullptr, &quadCount);

	CalCtx.chaperone.geometry.resize(quadCount);
	vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(&CalCtx.chaperone.geometry[0], &quadCount);
	vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&CalCtx.chaperone.standingCenter);
	vr::VRChaperoneSetup()->GetWorkingPlayAreaSize(&CalCtx.chaperone.playSpaceSize.v[0], &CalCtx.chaperone.playSpaceSize.v[1]);
	CalCtx.chaperone.valid = true;
}

void ApplyChaperoneBounds()
{
	vr::VRChaperoneSetup()->RevertWorkingCopy();
	vr::VRChaperoneSetup()->SetWorkingCollisionBoundsInfo(&CalCtx.chaperone.geometry[0], (uint32_t)CalCtx.chaperone.geometry.size());
	vr::VRChaperoneSetup()->SetWorkingStandingZeroPoseToRawTrackingPose(&CalCtx.chaperone.standingCenter);
	vr::VRChaperoneSetup()->SetWorkingPlayAreaSize(CalCtx.chaperone.playSpaceSize.v[0], CalCtx.chaperone.playSpaceSize.v[1]);
	vr::VRChaperoneSetup()->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
}

void DebugApplyRandomOffset() {
	protocol::Request req(protocol::RequestDebugOffset);
	SafeDriverSend(req);
}