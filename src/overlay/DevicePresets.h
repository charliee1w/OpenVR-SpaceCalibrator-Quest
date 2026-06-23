#pragma once

#include "Calibration.h"

struct DevicePresetDefinition {
	const char* id;
	const char* label;
	const char* description;
	const char* referenceTrackingSystem;
	const char* referenceModelHint;
	float maxPoseTimeSkew;
	float maxReferencePoseTimeOffset;
	float maxRelativeErrorThreshold;
	float jitterThreshold;
	float continuousSpikeThresholdM;
	int continuousFrozenFrameThreshold;
	float guardianDriftTransThresholdM;
	float guardianDriftYawThresholdDeg;
	int guardianDriftConfirmChecks;
	int guardianDriftCooldownFrames;
	bool pauseOnReferenceJitter;
	bool applyHeadModelToReference;
	bool rejectYawDriftPoses;
	bool trustTargetYaw;
	bool compensatePoseTimeOffset;
	bool autoRecalOnGuardianDrift;
	bool lockRelativePosition;
	bool enableStaticRecalibration;
	bool quashTargetInContinuous;
	bool autostartContinuous;
	CalibrationContext::Speed calibrationSpeed;
};

const DevicePresetDefinition* FindDevicePreset(const char* id);
const DevicePresetDefinition* FindDevicePresetByIndex(int index);
int DevicePresetCount();
const char* InferDevicePresetId(const std::string& chainName);

// Applies tuning + reference tracking hints. Does not change room cal, target devices, or chaperone geometry.
void ApplyDevicePreset(CalibrationContext& ctx, const char* presetId);