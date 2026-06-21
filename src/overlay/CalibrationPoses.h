#pragma once

#include "Calibration.h"
#include "Protocol.h"

// Calibration pose source contract (regression in contcal36/39):
//
// REFERENCE (Quest / inside-out HMD via VD oculus driver):
//   Prefer driver shmem (hook capture): includes poseTimeOffset, willDriftInYaw, head model.
//   VRSystem fallback when shmem is unavailable. Driver does not transform passthrough HMD.
//
// TARGET (lighthouse trackers / FBT):
//   Always driver shmem: raw pose captured BEFORE SetDeviceTransform is applied in the driver hook.
//   Never VRSystem while validProfile is true — VRSystem returns post-transform poses and breaks cal.
//
// VRSystem target fallback is allowed ONLY for first-time calibration (no saved profile / transform).

enum class CalPoseSource : uint8_t {
	Unset = 0,
	DriverShmem,
	VRSystemReference,
	VRSystemTargetFallback,
};

void RefreshDevicePosesForCalibration(
	CalibrationContext& ctx,
	protocol::DriverPoseShmem& shmem);

CalPoseSource GetLastCalPoseSource(int deviceId);

bool CalPoseIsTrackingOk(const vr::DriverPose_t& pose);