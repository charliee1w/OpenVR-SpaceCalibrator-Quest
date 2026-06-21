#include "stdafx.h"
#include "CalibrationPoses.h"

#include <openvr.h>
#include <cstring>

namespace {

CalPoseSource g_lastPoseSource[vr::k_unMaxTrackedDeviceCount] = {};

vr::HmdQuaternion_t Matrix34ToQuat(const vr::HmdMatrix34_t& m) {
	Eigen::Matrix3d rot;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			rot(i, j) = m.m[i][j];
		}
	}
	const Eigen::Quaterniond q(rot);
	return { q.w(), q.x(), q.y(), q.z() };
}

void TrackedPoseToDriverPose(const vr::TrackedDevicePose_t& tracked, vr::DriverPose_t& driver) {
	memset(&driver, 0, sizeof(driver));
	driver.poseIsValid = tracked.bPoseIsValid;
	driver.deviceIsConnected = tracked.bDeviceIsConnected;
	driver.result = tracked.eTrackingResult;
	driver.qWorldFromDriverRotation = { 1, 0, 0, 0 };
	driver.qDriverFromHeadRotation = { 1, 0, 0, 0 };
	driver.vecPosition[0] = tracked.mDeviceToAbsoluteTracking.m[0][3];
	driver.vecPosition[1] = tracked.mDeviceToAbsoluteTracking.m[1][3];
	driver.vecPosition[2] = tracked.mDeviceToAbsoluteTracking.m[2][3];
	driver.qRotation = Matrix34ToQuat(tracked.mDeviceToAbsoluteTracking);
	driver.vecVelocity[0] = tracked.vVelocity.v[0];
	driver.vecVelocity[1] = tracked.vVelocity.v[1];
	driver.vecVelocity[2] = tracked.vVelocity.v[2];
	driver.vecAngularVelocity[0] = tracked.vAngularVelocity.v[0];
	driver.vecAngularVelocity[1] = tracked.vAngularVelocity.v[1];
	driver.vecAngularVelocity[2] = tracked.vAngularVelocity.v[2];
}

void SetPoseSource(int deviceId, CalPoseSource source) {
	if (deviceId >= 0 && deviceId < vr::k_unMaxTrackedDeviceCount) {
		g_lastPoseSource[deviceId] = source;
	}
}

bool IsCalibrating(const CalibrationContext& ctx) {
	return ctx.state != CalibrationState::None
		&& ctx.state != CalibrationState::Editing;
}

} // namespace

bool CalPoseIsTrackingOk(const vr::DriverPose_t& pose) {
	return pose.poseIsValid
		&& pose.result == vr::ETrackingResult::TrackingResult_Running_OK;
}

CalPoseSource GetLastCalPoseSource(int deviceId) {
	if (deviceId < 0 || deviceId >= vr::k_unMaxTrackedDeviceCount) {
		return CalPoseSource::Unset;
	}
	return g_lastPoseSource[deviceId];
}

void RefreshDevicePosesForCalibration(CalibrationContext& ctx, protocol::DriverPoseShmem& shmem) {
	const int refId = ctx.referenceID >= 0 ? ctx.referenceID : (int)vr::k_unTrackedDeviceIndex_Hmd;
	const int targetId = ctx.targetID;

	shmem.ReadNewPoses([&](const protocol::DriverPoseShmem::AugmentedPose& augmented_pose) {
		if (augmented_pose.deviceId < 0 || augmented_pose.deviceId >= vr::k_unMaxTrackedDeviceCount) {
			return;
		}
		ctx.devicePoses[augmented_pose.deviceId] = augmented_pose.pose;
		ctx.devicePoseSampleTime[augmented_pose.deviceId] = augmented_pose.sample_time;
		SetPoseSource(augmented_pose.deviceId, CalPoseSource::DriverShmem);
	});

	if (targetId >= 0 && targetId < vr::k_unMaxTrackedDeviceCount) {
		vr::DriverPose_t cachedTarget = {};
		LARGE_INTEGER cachedTime = {};
		shmem.GetPose(targetId, cachedTarget, &cachedTime);
		if (CalPoseIsTrackingOk(cachedTarget)) {
			ctx.devicePoses[targetId] = cachedTarget;
			ctx.devicePoseSampleTime[targetId] = cachedTime;
			SetPoseSource(targetId, CalPoseSource::DriverShmem);
		}
	}

	if (!vr::VRSystem()) {
		return;
	}

	vr::TrackedDevicePose_t trackedPoses[vr::k_unMaxTrackedDeviceCount];
	vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
		vr::TrackingUniverseStanding, 0.0f, trackedPoses, vr::k_unMaxTrackedDeviceCount);

	if (refId >= 0 && refId < vr::k_unMaxTrackedDeviceCount) {
		TrackedPoseToDriverPose(trackedPoses[refId], ctx.devicePoses[refId]);
		QueryPerformanceCounter(&ctx.devicePoseSampleTime[refId]);
		SetPoseSource(refId, CalPoseSource::VRSystemReference);
	}

	if (targetId < 0 || targetId >= vr::k_unMaxTrackedDeviceCount || targetId == refId) {
		return;
	}

	if (CalPoseIsTrackingOk(ctx.devicePoses[targetId])) {
		// fall through to sample-time sync below
	}
	else if (!ctx.validProfile || !ctx.relativePosCalibrated) {
		// No active transform (or still bootstrapping ref-to-target): VRSystem matches raw tracking.
		TrackedPoseToDriverPose(trackedPoses[targetId], ctx.devicePoses[targetId]);
		QueryPerformanceCounter(&ctx.devicePoseSampleTime[targetId]);
		SetPoseSource(targetId, CalPoseSource::VRSystemTargetFallback);
	}
	else {
		if (IsCalibrating(ctx) && !ctx.warnedTargetPoseMissingFromShmem) {
			ctx.Log("Target pose missing from driver shmem (profile active). "
				"Waiting for raw tracker pose — do not use VRSystem here.\n");
			ctx.warnedTargetPoseMissingFromShmem = true;
		}

#ifdef _DEBUG
		if (IsCalibrating(ctx) && ctx.validProfile) {
			OutputDebugStringA(
				"SpaceCalibrator: target pose invalid with active profile; "
				"VRSystem fallback correctly blocked.\n");
		}
#endif
	}

	// VRSystem ref is re-polled every tick; driver target keeps its hook timestamp.
	// Without this, pose-time skew rejects every sample after ~50ms.
	if (refId >= 0 && refId < vr::k_unMaxTrackedDeviceCount
		&& targetId >= 0 && targetId < vr::k_unMaxTrackedDeviceCount
		&& GetLastCalPoseSource(refId) == CalPoseSource::VRSystemReference
		&& GetLastCalPoseSource(targetId) == CalPoseSource::DriverShmem
		&& CalPoseIsTrackingOk(ctx.devicePoses[targetId])
		&& ctx.devicePoseSampleTime[targetId].QuadPart != 0) {
		ctx.devicePoseSampleTime[refId] = ctx.devicePoseSampleTime[targetId];
	}
}