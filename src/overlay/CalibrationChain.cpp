#include "stdafx.h"
#include "CalibrationChain.h"
#include "Calibration.h"
#include "Configuration.h"
#include "VRState.h"
#include "CalibrationMetrics.h"
#include "IPCClient.h"

std::vector<CalibrationChain> CalChains;

void EnsureDefaultChain() {
	if (CalChains.empty()) {
		CalChains.emplace_back();
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
	for (int i = 0; i < (int)CalChains.size(); i++) {
		if (CalChains[i].valid && CalChains[i].targetTrackingSystem == trackingSystem) {
			return i;
		}
	}
	return -1;
}

bool AssignChainTargets(CalibrationChain& chain) {
	auto state = VRState::Load();

	if (chain.referenceID < 0) {
		chain.referenceID = state.FindDevice(
			chain.referenceStandby.trackingSystem,
			chain.referenceStandby.model,
			chain.referenceStandby.serial
		);
	}

	if (chain.targetID < 0) {
		chain.targetID = state.FindDevice(
			chain.targetStandby.trackingSystem,
			chain.targetStandby.model,
			chain.targetStandby.serial
		);
	}

	chain.slamReference = VRState::IsSlamTrackingSystem(chain.referenceTrackingSystem);
	return chain.referenceID >= 0 && chain.targetID >= 0;
}

bool CollectChainSample(CalibrationChain& chain, CalibrationContext& ctx) {
	return CollectSampleForChain(
		ctx,
		chain.calibration,
		chain.referenceID,
		chain.targetID,
		chain.continuousCalibrationOffset,
		chain.slamReference,
		chain.lockRelativePosition
	);
}

void ProcessContinuousChain(CalibrationChain& chain, CalibrationContext& ctx, double time) {
	if (!chain.continuousActive || !chain.valid) {
		return;
	}

	if (!AssignChainTargets(chain)) {
		return;
	}

	if (!CollectChainSample(chain, ctx)) {
		return;
	}

	while (chain.calibration.SampleCount() > CalCtx.SampleCount()) {
		chain.calibration.ShiftSample();
	}
	if (chain.calibration.SampleCount() < CalCtx.SampleCount()) {
		return;
	}

	if (CalCtx.pauseOnReferenceJitter && chain.calibration.SampleCount() >= 3 && !chain.slamReference) {
		if (chain.calibration.ReferenceJitter() > CalCtx.jitterThreshold) {
			return;
		}
	}

	bool lerp = false;
	chain.calibration.enableStaticRecalibration = CalCtx.enableStaticRecalibration;
	chain.calibration.lockRelativePosition = chain.lockRelativePosition;
	chain.calibration.ComputeIncremental(
		lerp,
		CalCtx.continuousCalibrationThreshold,
		CalCtx.maxRelativeErrorThreshold,
		CalCtx.ignoreOutliers
	);

	if (!chain.calibration.isValid()) {
		return;
	}

	chain.calibratedRotation = chain.calibration.EulerRotation();
	chain.calibratedTranslation = chain.calibration.Transformation().translation() * 100.0;
	chain.refToTargetPose = chain.calibration.RelativeTransformation();
	chain.relativePosCalibrated = chain.calibration.isRelativeTransformationCalibrated();

	ApplyChainCalibration(chain, lerp);
}

void StartContinuousChains() {
	SyncCalCtxToPrimaryChain();
	EnsureDefaultChain();

	for (auto& chain : CalChains) {
		if (!chain.valid) continue;
		if (!chain.autostartContinuous && &chain != &CalChains[0]) continue;

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

		char buf[128];
		snprintf(buf, sizeof buf, "Continuous chain started: %s -> %s\n",
			chain.referenceTrackingSystem.c_str(),
			chain.targetTrackingSystem.c_str());
		CalCtx.Log(buf);
	}

	Metrics::WriteLogAnnotation("StartContinuousChains");
}

void EndContinuousChains() {
	for (auto& chain : CalChains) {
		chain.continuousActive = false;
		chain.relativePosCalibrated = false;
	}
	SyncCalCtxToPrimaryChain();
	Metrics::WriteLogAnnotation("EndContinuousChains");
}