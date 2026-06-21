#include "stdafx.h"
#include "CalibrationChain.h"
#include "Calibration.h"
#include "Configuration.h"
#include "VRState.h"
#include "CalibrationMetrics.h"
#include "IPCClient.h"

std::vector<CalibrationChain> CalChains;

void InitGoreSetup(CalibrationContext& ctx) {
	EnsureDefaultChain();
	auto& chain = CalChains[0];

	const bool hadSavedCal = ctx.validProfile;
	const Eigen::Vector3d savedRot = ctx.calibratedRotation;
	const Eigen::Vector3d savedTrans = ctx.calibratedTranslation;
	const double savedScale = ctx.calibratedScale;
	const Eigen::AffineCompact3d savedRefToTarget = ctx.refToTargetPose;
	const bool savedRelPos = ctx.relativePosCalibrated;
	const StandbyDevice savedRefStandby = ctx.referenceStandby;
	const StandbyDevice savedTgtStandby = ctx.targetStandby;

	if (!hadSavedCal) {
		ctx.ApplyQuestProDefaults();
	} else {
		ctx.ApplySlamReferencePreset();
	}

	if (hadSavedCal) {
		ctx.calibratedRotation = savedRot;
		ctx.calibratedTranslation = savedTrans;
		ctx.calibratedScale = savedScale;
		ctx.refToTargetPose = savedRefToTarget;
		ctx.relativePosCalibrated = savedRelPos;
		ctx.validProfile = true;
		if (!savedRefStandby.serial.empty() || !savedRefStandby.model.empty()) {
			ctx.referenceStandby = savedRefStandby;
		}
		if (!savedTgtStandby.serial.empty() || !savedTgtStandby.model.empty()) {
			ctx.targetStandby = savedTgtStandby;
		}
	}

	chain.referenceTrackingSystem = ctx.referenceTrackingSystem;
	chain.targetTrackingSystem = ctx.targetTrackingSystem;
	chain.referenceStandby = ctx.referenceStandby;
	chain.targetStandby = ctx.targetStandby;
	chain.lockRelativePosition = ctx.lockRelativePosition;
	chain.slamReference = ctx.slamReference;
	chain.name = "Quest Pro -> Lighthouse FBT";
	if (!hadSavedCal) {
		chain.autostartContinuous = true;
	}

	SyncCalCtxToPrimaryChain();
}

void EnsureDefaultChain() {
	if (CalChains.empty()) {
		CalChains.emplace_back();
	}
	if (CalChains.size() > 1) {
		CalChains.resize(1);
	}
}

void SyncCalCtxToPrimaryChain() {
	EnsureDefaultChain();
	auto& chain = CalChains[0];
	chain.referenceTrackingSystem = CalCtx.referenceTrackingSystem;
	chain.targetTrackingSystem = CalCtx.targetTrackingSystem;
	chain.referenceStandby = CalCtx.referenceStandby;
	chain.targetStandby = CalCtx.targetStandby;
	chain.calibratedRotation = CalCtx.calibratedRotation;
	chain.calibratedTranslation = CalCtx.calibratedTranslation;
	chain.calibratedScale = CalCtx.calibratedScale;
	chain.refToTargetPose = CalCtx.refToTargetPose;
	chain.relativePosCalibrated = CalCtx.relativePosCalibrated;
	chain.lockRelativePosition = CalCtx.lockRelativePosition;
	chain.valid = CalCtx.validProfile;
	chain.referenceID = CalCtx.referenceID;
	chain.targetID = CalCtx.targetID;
	chain.slamReference = CalCtx.slamReference;
	chain.continuousCalibrationOffset = CalCtx.continuousCalibrationOffset;
	chain.continuousActive = CalCtx.state == CalibrationState::Continuous;
	chain.autostartContinuous =
		CalCtx.state == CalibrationState::Continuous
		|| CalCtx.state == CalibrationState::ContinuousStandby;
}

void SyncPrimaryChainToCalCtx() {
	EnsureDefaultChain();
	const auto& chain = CalChains[0];
	CalCtx.referenceTrackingSystem = chain.referenceTrackingSystem;
	CalCtx.targetTrackingSystem = chain.targetTrackingSystem;
	CalCtx.referenceStandby = chain.referenceStandby;
	CalCtx.targetStandby = chain.targetStandby;
	CalCtx.calibratedRotation = chain.calibratedRotation;
	CalCtx.calibratedTranslation = chain.calibratedTranslation;
	CalCtx.calibratedScale = chain.calibratedScale;
	CalCtx.refToTargetPose = chain.refToTargetPose;
	CalCtx.relativePosCalibrated = chain.relativePosCalibrated;
	CalCtx.lockRelativePosition = chain.lockRelativePosition;
	CalCtx.validProfile = chain.valid;
	CalCtx.referenceID = chain.referenceID;
	CalCtx.targetID = chain.targetID;
	CalCtx.slamReference = chain.slamReference;
	CalCtx.continuousCalibrationOffset = chain.continuousCalibrationOffset;
}

int FindChainIndexForTargetSystem(const std::string& trackingSystem) {
	EnsureDefaultChain();
	if (CalChains[0].valid && CalChains[0].targetTrackingSystem == trackingSystem) {
		return 0;
	}
	return -1;
}

bool AssignChainTargets(CalibrationChain& chain) {
	auto state = VRState::Load();

	const int resolvedReference = state.ResolveStandbyDevice(
		chain.referenceStandby, chain.referenceTrackingSystem);
	if (resolvedReference >= 0
		&& (chain.referenceID < 0 || !VRState::IsDeviceConnected(chain.referenceID))) {
		chain.referenceID = resolvedReference;
	}

	const int resolvedTarget = state.ResolveStandbyDevice(
		chain.targetStandby, chain.targetTrackingSystem);
	if (resolvedTarget >= 0
		&& (chain.targetID < 0 || !VRState::IsDeviceConnected(chain.targetID))) {
		chain.targetID = resolvedTarget;
	}

	chain.slamReference = VRState::IsSlamTrackingSystem(chain.referenceTrackingSystem);
	return chain.referenceID >= 0 && chain.targetID >= 0;
}

void StartContinuousChains() {
	SyncCalCtxToPrimaryChain();
	EnsureDefaultChain();
	auto& chain = CalChains[0];

	AssignChainTargets(chain);
	if (chain.slamReference) {
		CalCtx.ApplySlamReferencePreset();
	}
	chain.continuousActive = true;
	chain.calibration.Clear();
	chain.calibration.ResetContinuousGuards();
	chain.calibration.setRelativeTransformation(chain.refToTargetPose, chain.relativePosCalibrated);
	chain.calibration.lockRelativePosition = chain.lockRelativePosition;
	chain.calibration.enableStaticRecalibration = CalCtx.enableStaticRecalibration;
	if (chain.valid && chain.relativePosCalibrated) {
		chain.calibration.SeedFromProfile(chain.calibratedRotation, chain.calibratedTranslation);
	}

	char buf[128];
	snprintf(buf, sizeof buf, "Continuous cal: %s -> %s\n",
		chain.referenceTrackingSystem.c_str(),
		chain.targetTrackingSystem.c_str());
	CalCtx.Log(buf);

	Metrics::WriteLogAnnotation("StartContinuousChains");
}

void EndContinuousChains() {
	EnsureDefaultChain();
	CalChains[0].continuousActive = false;
	SyncCalCtxToPrimaryChain();
	Metrics::WriteLogAnnotation("EndContinuousChains");
}