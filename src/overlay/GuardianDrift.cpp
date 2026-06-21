#include "stdafx.h"
#include "GuardianDrift.h"
#include "Calibration.h"
#include "CalibrationCalc.h"
#include "CalibrationMetrics.h"
#include "Configuration.h"
#include "VRState.h"

#include <cmath>

namespace {

	Eigen::Matrix3d Matrix34ToRot(const vr::HmdMatrix34_t& m) {
		Eigen::Matrix3d rot;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				rot(i, j) = m.m[i][j];
			}
		}
		return rot;
	}

	Eigen::Vector3d Matrix34ToTrans(const vr::HmdMatrix34_t& m) {
		return Eigen::Vector3d(m.m[0][3], m.m[1][3], m.m[2][3]);
	}

}

bool ReadLiveChaperoneSnapshot(LiveChaperoneSnapshot& out, uint32_t hmdDeviceId) {
	if (!vr::VRChaperoneSetup() || !vr::VRSystem()) {
		return false;
	}

	out = LiveChaperoneSnapshot();

	uint32_t quadCount = 0;
	vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(nullptr, &quadCount);
	out.quadCount = quadCount;

	vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&out.standingCenter);
	vr::VRChaperoneSetup()->GetWorkingPlayAreaSize(&out.playAreaSize.v[0], &out.playAreaSize.v[1]);

	vr::ETrackedPropertyError err = vr::TrackedProp_Success;
	out.universeId = vr::VRSystem()->GetUint64TrackedDeviceProperty(
		hmdDeviceId,
		vr::Prop_CurrentUniverseId_Uint64,
		&err
	);
	if (err != vr::TrackedProp_Success) {
		out.universeId = 0;
	}

	out.valid = true;
	return true;
}

double StandingCenterTranslationDriftM(const vr::HmdMatrix34_t& stored, const vr::HmdMatrix34_t& live) {
	return (Matrix34ToTrans(stored) - Matrix34ToTrans(live)).norm();
}

double StandingCenterYawDriftRad(const vr::HmdMatrix34_t& stored, const vr::HmdMatrix34_t& live) {
	const Eigen::Matrix3d storedRot = Matrix34ToRot(stored);
	const Eigen::Matrix3d liveRot = Matrix34ToRot(live);
	const Eigen::Matrix3d delta = storedRot.transpose() * liveRot;
	return std::abs(Eigen::AngleAxisd(delta).angle());
}

bool HandleGuardianDrift(CalibrationContext& ctx, GuardianDriftState& state, double time, CalibrationCalc& primaryCalc) {
	if (!ctx.chaperone.valid || !ctx.slamReference) {
		return false;
	}

	if (state.driftCooldownFrames > 0) {
		state.driftCooldownFrames--;
		return false;
	}

	if ((time - state.lastCheckTime) < 0.5) {
		return false;
	}
	state.lastCheckTime = time;

	LiveChaperoneSnapshot live;
	if (!ReadLiveChaperoneSnapshot(live, vr::k_unTrackedDeviceIndex_Hmd)) {
		return false;
	}

	bool driftDetected = false;
	std::string driftReason;

	if (state.lastSnapshot.valid && live.universeId != 0 && state.lastSnapshot.universeId != 0
		&& live.universeId != state.lastSnapshot.universeId) {
		driftDetected = true;
		driftReason = "tracking universe changed";
	}

	const double transDrift = StandingCenterTranslationDriftM(ctx.chaperone.standingCenter, live.standingCenter);
	const double yawDrift = StandingCenterYawDriftRad(ctx.chaperone.standingCenter, live.standingCenter);
	if (transDrift > ctx.guardianDriftTransThresholdM
		|| yawDrift > ctx.guardianDriftYawThresholdRad) {
		state.pendingDriftChecks++;
		if (state.pendingDriftChecks >= ctx.guardianDriftConfirmChecks) {
			driftDetected = true;
			if (!driftReason.empty()) driftReason += "; ";
			driftReason += "guardian origin shifted";
		}
	} else {
		state.pendingDriftChecks = 0;
	}

	if (live.quadCount != ctx.chaperone.geometry.size()) {
		if (ctx.chaperone.autoApply) {
			ApplyChaperoneBounds();
			ctx.Log("Guardian geometry reset detected, restored profile chaperone\n");
			Metrics::WriteLogAnnotation("GuardianGeometryRestore");
		}
	}

	if (!driftDetected) {
		state.lastSnapshot = live;
		return false;
	}

	state.pendingDriftChecks = 0;

	char buf[256];
	snprintf(buf, sizeof buf, "Guardian drift detected (%s): trans=%.1fmm yaw=%.1fdeg\n",
		driftReason.c_str(), transDrift * 1000.0, yawDrift * 180.0 / EIGEN_PI);
	ctx.Log(buf);
	Metrics::WriteLogAnnotation("GuardianDrift");

	if (ctx.chaperone.autoApply) {
		LoadChaperoneBounds();
		SaveProfile(ctx);
		ctx.Log("Updated profile chaperone from live guardian\n");
	}

	primaryCalc.Clear();
	primaryCalc.ResetContinuousGuards();
	state.driftCooldownFrames = ctx.guardianDriftCooldownFrames;
	state.lastSnapshot = live;
	return true;
}