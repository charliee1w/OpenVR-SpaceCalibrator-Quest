#include "stdafx.h"
#include "DevicePresets.h"
#include "CalibrationChain.h"
#include "VRState.h"

#include <cstring>

namespace {
constexpr float kPi = static_cast<float>(EIGEN_PI);

DevicePresetDefinition MakeSlamPreset(
	const char* id,
	const char* label,
	const char* description,
	const char* referenceTrackingSystem,
	const char* referenceModelHint,
	float maxSkew,
	float maxOffset,
	float maxRelErr,
	bool questProStyleGuardian = true)
{
	DevicePresetDefinition preset{};
	preset.id = id;
	preset.label = label;
	preset.description = description;
	preset.referenceTrackingSystem = referenceTrackingSystem;
	preset.referenceModelHint = referenceModelHint;
	preset.maxPoseTimeSkew = maxSkew;
	preset.maxReferencePoseTimeOffset = maxOffset;
	preset.maxRelativeErrorThreshold = maxRelErr;
	preset.jitterThreshold = 0.15f;
	preset.continuousSpikeThresholdM = 0.05f;
	preset.continuousFrozenFrameThreshold = 5;
	preset.guardianDriftTransThresholdM = questProStyleGuardian ? 0.035f : 0.035f;
	preset.guardianDriftYawThresholdDeg = 5.0f;
	preset.guardianDriftConfirmChecks = 3;
	preset.guardianDriftCooldownFrames = 60;
	preset.pauseOnReferenceJitter = false;
	preset.applyHeadModelToReference = true;
	preset.rejectYawDriftPoses = true;
	preset.trustTargetYaw = true;
	preset.compensatePoseTimeOffset = true;
	preset.autoRecalOnGuardianDrift = false;
	preset.lockRelativePosition = true;
	preset.enableStaticRecalibration = true;
	preset.quashTargetInContinuous = true;
	preset.autostartContinuous = true;
	preset.calibrationSpeed = CalibrationContext::SLOW;
	return preset;
}

const DevicePresetDefinition kPresets[] = {
	{
		"custom",
		"Custom (manual sliders)",
		"Keep your current slider values. Select a preset below to load tuned defaults.",
		"", "",
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		false, false, false, false, false, false,
		false, false, false, false,
		CalibrationContext::FAST,
	},
	MakeSlamPreset(
		"quest_vd",
		"Quest / Quest 2 + Virtual Desktop",
		"Wireless Meta headset via VD into SteamVR (oculus). WiFi latency tuning.",
		"oculus", "Meta Quest",
		0.08f, 0.04f, 0.008f),
	MakeSlamPreset(
		"quest_pro_vd",
		"Quest Pro + Virtual Desktop",
		"Quest Pro via VD. Uses Quest Pro device matching when resolving the HMD.",
		"oculus", "Meta Quest Pro",
		0.08f, 0.04f, 0.008f),
	MakeSlamPreset(
		"quest_link",
		"Quest + Wired Link",
		"Meta headset via Oculus Link / Air Link with lower streaming latency.",
		"oculus", "Meta Quest",
		0.05f, 0.02f, 0.008f),
	MakeSlamPreset(
		"pico_alvr",
		"Pico + ALVR",
		"Pico inside-out HMD via ALVR into SteamVR (pico tracking system).",
		"pico", "Pico",
		0.06f, 0.03f, 0.008f),
	MakeSlamPreset(
		"pico_vd",
		"Pico + Virtual Desktop",
		"Pico via Virtual Desktop when exposed as pico in SteamVR.",
		"pico", "Pico",
		0.07f, 0.035f, 0.008f),
	MakeSlamPreset(
		"winmr_vd",
		"Windows MR + Virtual Desktop",
		"WMR-class inside-out HMD via VD (winmr / holographic).",
		"winmr", "",
		0.07f, 0.035f, 0.01f, false),
	MakeSlamPreset(
		"slam_tuned",
		"SLAM tuned (validated winner)",
		"P4-validated Quest lighthouse profile: lock-relative, autostart, quash target.",
		"oculus", "Meta Quest Pro",
		0.08f, 0.04f, 0.008f),
	{
		"upstream_pcvr",
		"PCVR / Index (upstream defaults)",
		"Lighthouse or PCVR reference — upstream SpaceCal slider defaults, not SLAM tuning.",
		"", "",
		0.05f, 0.04f, 0.005f,
		0.08f, 0.06f, 6,
		0.02f, 3.0f, 2, 20,
		true, false, false, false, false, true,
		false, false, false, false,
		CalibrationContext::FAST,
	},
};

static_assert(sizeof(kPresets) / sizeof(kPresets[0]) > 1, "device presets required");
}

const DevicePresetDefinition* FindDevicePreset(const char* id) {
	if (!id || !id[0]) {
		return &kPresets[0];
	}
	for (const auto& preset : kPresets) {
		if (std::strcmp(preset.id, id) == 0) {
			return &preset;
		}
	}
	return nullptr;
}

const DevicePresetDefinition* FindDevicePresetByIndex(int index) {
	if (index < 0 || index >= static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]))) {
		return nullptr;
	}
	return &kPresets[index];
}

int DevicePresetCount() {
	return static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
}

const char* InferDevicePresetId(const std::string& chainName) {
	if (chainName == "quest-vd-lighthouse") return "quest_vd";
	if (chainName == "quest-pro-vd-lighthouse") return "quest_pro_vd";
	if (chainName == "quest-link-lighthouse") return "quest_link";
	if (chainName == "pico-alvr-lighthouse") return "pico_alvr";
	if (chainName == "pico-vd-lighthouse") return "pico_vd";
	if (chainName.empty()) return "custom";
	return "custom";
}

void ApplyDevicePreset(CalibrationContext& ctx, const char* presetId) {
	const DevicePresetDefinition* preset = FindDevicePreset(presetId);
	if (!preset) {
		return;
	}

	ctx.devicePresetId = preset->id;
	if (std::strcmp(preset->id, "custom") == 0) {
		return;
	}

	if (preset->referenceTrackingSystem[0] != '\0') {
		ctx.referenceTrackingSystem = preset->referenceTrackingSystem;
		ctx.referenceStandby.trackingSystem = preset->referenceTrackingSystem;
	}
	if (preset->referenceModelHint[0] != '\0'
		&& ctx.referenceStandby.serial.empty()) {
		ctx.referenceStandby.model = preset->referenceModelHint;
	}

	ctx.maxPoseTimeSkew = preset->maxPoseTimeSkew;
	ctx.maxReferencePoseTimeOffset = preset->maxReferencePoseTimeOffset;
	ctx.maxRelativeErrorThreshold = preset->maxRelativeErrorThreshold;
	ctx.jitterThreshold = preset->jitterThreshold;
	ctx.continuousSpikeThresholdM = preset->continuousSpikeThresholdM;
	ctx.continuousFrozenFrameThreshold = preset->continuousFrozenFrameThreshold;
	ctx.guardianDriftTransThresholdM = preset->guardianDriftTransThresholdM;
	ctx.guardianDriftYawThresholdRad = preset->guardianDriftYawThresholdDeg * kPi / 180.0f;
	ctx.guardianDriftConfirmChecks = preset->guardianDriftConfirmChecks;
	ctx.guardianDriftCooldownFrames = preset->guardianDriftCooldownFrames;
	ctx.pauseOnReferenceJitter = preset->pauseOnReferenceJitter;
	ctx.applyHeadModelToReference = preset->applyHeadModelToReference;
	ctx.rejectYawDriftPoses = preset->rejectYawDriftPoses;
	ctx.trustTargetYaw = preset->trustTargetYaw;
	ctx.compensatePoseTimeOffset = preset->compensatePoseTimeOffset;
	ctx.autoRecalOnGuardianDrift = preset->autoRecalOnGuardianDrift;
	ctx.lockRelativePosition = preset->lockRelativePosition;
	ctx.enableStaticRecalibration = preset->enableStaticRecalibration;
	ctx.quashTargetInContinuous = preset->quashTargetInContinuous;
	ctx.calibrationSpeed = preset->calibrationSpeed;
	ctx.ReconcileContinuousRefinementModes();

	ctx.slamReference = VRState::IsSlamTrackingSystem(ctx.referenceTrackingSystem);
	if (ctx.slamReference) {
		ctx.autoRecalOnGuardianDrift = false;
	}

	EnsureDefaultChain();
	if (!CalChains.empty()) {
		CalChains[0].name = preset->id;
		CalChains[0].referenceTrackingSystem = ctx.referenceTrackingSystem;
		if (preset->autostartContinuous) {
			CalChains[0].autostartContinuous = true;
		}
		CalChains[0].lockRelativePosition = ctx.lockRelativePosition;
	}

	char buf[256];
	snprintf(buf, sizeof buf, "DevicePresetApplied:%s\n", preset->id);
	ctx.Log(buf);
}