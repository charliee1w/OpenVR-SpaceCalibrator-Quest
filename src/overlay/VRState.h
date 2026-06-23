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

struct SlamReferenceMatch {
	int deviceId = -1;
	std::string trackingSystem;
	std::string model;
	std::string serial;
};

struct VRState
{
	std::vector<std::string> trackingSystems;
	std::vector<VRDevice> devices;

	[[nodiscard]] int FindDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const;
	[[nodiscard]] int FindQuestProHmd() const;
	// Any connected SLAM HMD (Quest/VD, Pico, etc.); prefers preferredTrackingSystem when set.
	[[nodiscard]] int FindSlamReferenceHmd(const std::string& preferredTrackingSystem = "") const;
	[[nodiscard]] SlamReferenceMatch ResolveSlamReference(
		const StandbyDevice& standby,
		const std::string& referenceTrackingSystem
	) const;
	[[nodiscard]] int ResolveStandbyDevice(
		const StandbyDevice& standby,
		const std::string& fallbackTrackingSystem
	) const;

	static bool IsSlamTrackingSystem(const std::string& trackingSystem);
	static bool IsDeviceConnected(int deviceId);

	static VRState Load();
};