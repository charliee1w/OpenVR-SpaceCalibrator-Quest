#pragma once

#include <openvr.h>
#include <Eigen/Dense>
#include <cmath>

enum class TrackingQuality {
	Good,
	Degraded,
	Frozen,
	Invalid
};

struct DeviceTrackingState {
	TrackingQuality quality = TrackingQuality::Invalid;
	Eigen::Vector3d lastPosition = Eigen::Vector3d::Zero();
	int unchangedFrames = 0;
	double lastPoseTimeOffset = 0.0;
};

inline double Vec3Norm(const double v[3]) {
	return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline bool Vec3NearZero(const double v[3], double eps = 1e-6) {
	return std::abs(v[0]) < eps && std::abs(v[1]) < eps && std::abs(v[2]) < eps;
}

inline bool Vec3Equal(const double a[3], const Eigen::Vector3d& b, double eps = 1e-5) {
	return std::abs(a[0] - b.x()) < eps && std::abs(a[1] - b.y()) < eps && std::abs(a[2] - b.z()) < eps;
}

inline TrackingQuality EvaluateDeviceTracking(
	const vr::DriverPose_t& pose,
	DeviceTrackingState& state,
	bool slamDevice,
	double maxPoseTimeOffset,
	int frozenFrameThreshold)
{
	const double pos[3] = { pose.vecPosition[0], pose.vecPosition[1], pose.vecPosition[2] };

	if (!pose.poseIsValid
		|| pose.result != vr::ETrackingResult::TrackingResult_Running_OK
		|| Vec3NearZero(pos, 1e-4)) {
		state.quality = TrackingQuality::Invalid;
		state.unchangedFrames = 0;
		return state.quality;
	}

	if (Vec3Equal(pos, state.lastPosition)) {
		state.unchangedFrames++;
	} else {
		state.unchangedFrames = 0;
		state.lastPosition = Eigen::Vector3d(pos[0], pos[1], pos[2]);
	}

	state.lastPoseTimeOffset = pose.poseTimeOffset;

	const bool lowMotion = Vec3Norm(pose.vecVelocity) < 1e-3 && Vec3Norm(pose.vecAngularVelocity) < 1e-3;
	if (state.unchangedFrames >= frozenFrameThreshold && lowMotion) {
		state.quality = TrackingQuality::Frozen;
		return state.quality;
	}

	if (slamDevice && (pose.willDriftInYaw || std::abs(pose.poseTimeOffset) > maxPoseTimeOffset)) {
		state.quality = TrackingQuality::Degraded;
		return state.quality;
	}

	state.quality = TrackingQuality::Good;
	return state.quality;
}

inline bool ShouldSkipCalibrationTick(TrackingQuality quality) {
	return quality == TrackingQuality::Invalid || quality == TrackingQuality::Frozen;
}

inline bool ShouldRejectCalibrationSample(TrackingQuality quality) {
	return quality != TrackingQuality::Good;
}