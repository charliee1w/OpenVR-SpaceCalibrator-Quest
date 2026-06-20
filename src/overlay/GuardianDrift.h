#pragma once

#include <openvr.h>
#include <cstdint>

struct CalibrationContext;
class CalibrationCalc;

struct LiveChaperoneSnapshot {
	bool valid = false;
	uint32_t quadCount = 0;
	vr::HmdMatrix34_t standingCenter = {};
	vr::HmdVector2_t playAreaSize = { 0.0f, 0.0f };
	uint64_t universeId = 0;
};

struct GuardianDriftState {
	LiveChaperoneSnapshot lastSnapshot;
	double lastCheckTime = 0.0;
	int driftCooldownFrames = 0;
	int pendingDriftChecks = 0;
};

bool ReadLiveChaperoneSnapshot(LiveChaperoneSnapshot& out, uint32_t hmdDeviceId);
double StandingCenterTranslationDriftM(const vr::HmdMatrix34_t& stored, const vr::HmdMatrix34_t& live);
double StandingCenterYawDriftRad(const vr::HmdMatrix34_t& stored, const vr::HmdMatrix34_t& live);
bool HandleGuardianDrift(CalibrationContext& ctx, GuardianDriftState& state, double time, CalibrationCalc& primaryCalc);