#pragma once

#include <string>
#include <vector>
#include <openvr.h>

struct VRDevice
{
	int id = -1;
	vr::TrackedDeviceClass deviceClass = vr::TrackedDeviceClass::TrackedDeviceClass_Invalid;
	std::string model = "";
	std::string serial = "";
	std::string trackingSystem = "";
	vr::ETrackedControllerRole controllerRole = vr::ETrackedControllerRole::TrackedControllerRole_Invalid;
};

struct StandbyDevice;

struct VRState
{
	std::vector<std::string> trackingSystems;
	std::vector<VRDevice> devices;

	[[nodiscard]] int FindDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const;
	// Prefer Quest Pro model string; fall back to any oculus HMD.
	[[nodiscard]] int FindQuestProHmd() const;
	[[nodiscard]] int ResolveStandbyDevice(
		const StandbyDevice& standby,
		const std::string& fallbackTrackingSystem
	) const;

	static bool IsSlamTrackingSystem(const std::string& trackingSystem);
	static bool IsDeviceConnected(int deviceId);

	static VRState Load();
};