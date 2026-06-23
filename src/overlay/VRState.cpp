#include "stdafx.h"
#include "VRState.h"
#include "Calibration.h"

#include <algorithm>

VRState VRState::Load()
{
	VRState state;
	auto& trackingSystems = state.trackingSystems;

	char buffer[vr::k_unMaxPropertyStringSize] = {};

	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id)
	{
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
		if (deviceClass == vr::TrackedDeviceClass_Invalid)
			continue;

		if (deviceClass != vr::TrackedDeviceClass_TrackingReference)
		{
			vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize, &err);

			if (err == vr::TrackedProp_Success)
			{
				std::string system(buffer);

				if (deviceClass == vr::TrackedDeviceClass_HMD && system == "aapvr") {
					vr::HmdMatrix34_t eyeToHeadLeft = vr::VRSystem()->GetEyeToHeadTransform(vr::Eye_Left);
					bool isCrystalHmd =
						eyeToHeadLeft.m[0][0] == 1 && eyeToHeadLeft.m[0][1] == 0 && eyeToHeadLeft.m[0][2] == 0 &&
						eyeToHeadLeft.m[1][0] == 0 && eyeToHeadLeft.m[1][1] == 1 && eyeToHeadLeft.m[1][2] == 0 && eyeToHeadLeft.m[1][3] == 0 &&
						eyeToHeadLeft.m[2][0] == 0 && eyeToHeadLeft.m[2][1] == 0 && eyeToHeadLeft.m[2][2] == 1 && eyeToHeadLeft.m[2][3] == 0;

					if (isCrystalHmd) {
						system = "Pimax Crystal HMD";
					}
				} else if (deviceClass == vr::TrackedDeviceClass_Controller && system == "oculus") {
					vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize, &err);
					std::string renderModel(buffer);
					vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_ConnectedWirelessDongle_String, buffer, vr::k_unMaxPropertyStringSize, &err);
					std::string connectedWirelessDongle(buffer);

					if (renderModel.find("{aapvr}") != std::string::npos &&
						renderModel.find("crystal") != std::string::npos &&
						connectedWirelessDongle.find("lighthouse") != std::string::npos) {
						system = "Pimax Crystal Controllers";
					}
				}

				auto existing = std::find(trackingSystems.begin(), trackingSystems.end(), system);
				if (existing != trackingSystems.end())
				{
					if (deviceClass == vr::TrackedDeviceClass_HMD)
					{
						trackingSystems.erase(existing);
						trackingSystems.insert(trackingSystems.begin(), system);
					}
				}
				else
				{
					trackingSystems.push_back(system);
				}

				VRDevice device;
				device.id = id;
				device.deviceClass = deviceClass;
				device.trackingSystem = system;

				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_ModelNumber_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				device.model = std::string(buffer);

				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				device.serial = std::string(buffer);

				device.controllerRole = (vr::ETrackedControllerRole)vr::VRSystem()->GetInt32TrackedDeviceProperty(id, vr::Prop_ControllerRoleHint_Int32, &err);

				state.devices.push_back(device);
			}
			else
			{
				printf("failed to get tracking system name for id %d\n", id);
			}
		}
	}

	return state;
}

bool VRState::IsDeviceConnected(int deviceId) {
	if (deviceId < 0 || !vr::VRSystem()) {
		return false;
	}
	return vr::VRSystem()->GetTrackedDeviceClass(deviceId) != vr::TrackedDeviceClass_Invalid;
}

bool VRState::IsSlamTrackingSystem(const std::string& trackingSystem) {
	return trackingSystem == "oculus"
		|| trackingSystem == "holographic"
		|| trackingSystem == "winmr"
		|| trackingSystem == "pico"
		|| trackingSystem == "nreal"
		|| trackingSystem == "euler";
}

int VRState::ResolveStandbyDevice(
	const StandbyDevice& standby,
	const std::string& fallbackTrackingSystem
) const {
	if (standby.serial.empty() && standby.model.empty()) {
		return -1;
	}
	const std::string& trackingSystem = !standby.trackingSystem.empty()
		? standby.trackingSystem
		: fallbackTrackingSystem;
	if (trackingSystem.empty()) {
		return -1;
	}
	return FindDevice(trackingSystem, standby.model, standby.serial);
}

namespace {
	bool ModelsMatchQuestPro(const std::string& deviceModel, const std::string& wantedModel) {
		if (deviceModel == wantedModel) {
			return true;
		}
		if (wantedModel.find("Quest Pro") != std::string::npos
			&& deviceModel.find("Quest Pro") != std::string::npos) {
			return true;
		}
		if (wantedModel.find("Quest") != std::string::npos
			&& deviceModel.find("Quest") != std::string::npos) {
			return true;
		}
		return false;
	}

	bool SerialMatchesQuestLink(const std::string& deviceSerial, const std::string& wantedSerial) {
		if (deviceSerial.empty() || wantedSerial.empty()) {
			return false;
		}
		if (deviceSerial == wantedSerial) {
			return true;
		}
		const bool deviceIsVrLink = deviceSerial.rfind("VRLINK", 0) == 0;
		const bool wantedIsVrLink = wantedSerial.rfind("VRLINK", 0) == 0;
		return deviceIsVrLink && wantedIsVrLink;
	}
}

namespace {
	const VRDevice* FindDeviceById(const VRState& state, int id) {
		for (const auto& device : state.devices) {
			if (device.id == id) {
				return &device;
			}
		}
		return nullptr;
	}

	void FillSlamReferenceMatchFromId(int id, SlamReferenceMatch& match) {
		match.deviceId = id;
		if (!vr::VRSystem() || id < 0) {
			return;
		}

		char buffer[vr::k_unMaxPropertyStringSize] = {};
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;

		vr::VRSystem()->GetStringTrackedDeviceProperty(
			id, vr::Prop_TrackingSystemName_String, buffer, sizeof(buffer), &err);
		if (err == vr::TrackedProp_Success) {
			match.trackingSystem = buffer;
		}

		vr::VRSystem()->GetStringTrackedDeviceProperty(
			id, vr::Prop_ModelNumber_String, buffer, sizeof(buffer), &err);
		if (err == vr::TrackedProp_Success) {
			match.model = buffer;
		}

		vr::VRSystem()->GetStringTrackedDeviceProperty(
			id, vr::Prop_SerialNumber_String, buffer, sizeof(buffer), &err);
		if (err == vr::TrackedProp_Success) {
			match.serial = buffer;
		}
	}
}

SlamReferenceMatch VRState::ResolveSlamReference(
	const StandbyDevice& standby,
	const std::string& referenceTrackingSystem
) const {
	SlamReferenceMatch match;
	int resolved = ResolveStandbyDevice(standby, referenceTrackingSystem);

	if (resolved >= 0) {
		const VRDevice* device = FindDeviceById(*this, resolved);
		if (device != nullptr && device->deviceClass != vr::TrackedDeviceClass_HMD
			&& (referenceTrackingSystem.empty() || IsSlamTrackingSystem(referenceTrackingSystem))) {
			resolved = -1;
		}
	}

	if (resolved < 0) {
		resolved = FindSlamReferenceHmd(referenceTrackingSystem);
	}
	if (resolved < 0 && referenceTrackingSystem == "oculus") {
		resolved = FindQuestProHmd();
	}
	if (resolved < 0) {
		return match;
	}

	const VRDevice* device = FindDeviceById(*this, resolved);
	if (device != nullptr) {
		match.deviceId = device->id;
		match.trackingSystem = device->trackingSystem;
		match.model = device->model;
		match.serial = device->serial;
	} else {
		FillSlamReferenceMatchFromId(resolved, match);
	}

	if (match.trackingSystem.empty() && !referenceTrackingSystem.empty()) {
		match.trackingSystem = referenceTrackingSystem;
	}

	return match;
}

int VRState::FindSlamReferenceHmd(const std::string& preferredTrackingSystem) const {
	int preferredId = -1;
	int anySlamHmd = -1;

	for (const auto& device : devices) {
		if (device.deviceClass != vr::TrackedDeviceClass_HMD) {
			continue;
		}
		if (!IsSlamTrackingSystem(device.trackingSystem)) {
			continue;
		}
		if (anySlamHmd < 0) {
			anySlamHmd = device.id;
		}
		if (!preferredTrackingSystem.empty() && device.trackingSystem == preferredTrackingSystem) {
			preferredId = device.id;
			break;
		}
	}

	if (preferredId >= 0) {
		return preferredId;
	}
	if (anySlamHmd >= 0) {
		return anySlamHmd;
	}

	// OpenVR HMD index is usually 0; accept it when class/tracking system match.
	if (vr::VRSystem()
		&& vr::VRSystem()->GetTrackedDeviceClass(vr::k_unTrackedDeviceIndex_Hmd) == vr::TrackedDeviceClass_HMD) {
		char buffer[vr::k_unMaxPropertyStringSize] = {};
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		vr::VRSystem()->GetStringTrackedDeviceProperty(
			vr::k_unTrackedDeviceIndex_Hmd,
			vr::Prop_TrackingSystemName_String,
			buffer,
			sizeof(buffer),
			&err);
		if (err == vr::TrackedProp_Success && IsSlamTrackingSystem(buffer)) {
			if (preferredTrackingSystem.empty() || preferredTrackingSystem == buffer) {
				return vr::k_unTrackedDeviceIndex_Hmd;
			}
		}
	}

	return -1;
}

int VRState::FindQuestProHmd() const {
	int fallbackOculusHmd = -1;
	for (const auto& device : devices) {
		if (device.deviceClass != vr::TrackedDeviceClass_HMD
			|| device.trackingSystem != "oculus") {
			continue;
		}
		if (device.model.find("Quest Pro") != std::string::npos) {
			return device.id;
		}
		if (fallbackOculusHmd < 0) {
			fallbackOculusHmd = device.id;
		}
	}
	if (fallbackOculusHmd >= 0) {
		return fallbackOculusHmd;
	}
	return FindSlamReferenceHmd("oculus");
}

int VRState::FindDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const {
	for (int i = 0; i < devices.size(); i++) {
		const auto& device = devices[i];

		uint8_t matches = 0;

		if (!model.empty()) {
			if (device.deviceClass == vr::TrackedDeviceClass_HMD
				&& device.trackingSystem == trackingSystem
				&& ModelsMatchQuestPro(device.model, model)) {
				matches++;
			} else if (device.model == model) {
				matches++;
			}
		}
		if (!serial.empty()) {
			if (device.serial == serial) {
				matches++;
			} else if (device.deviceClass == vr::TrackedDeviceClass_HMD
				&& device.trackingSystem == trackingSystem
				&& SerialMatchesQuestLink(device.serial, serial)) {
				matches++;
			}
		}

		if (device.trackingSystem == trackingSystem &&
			((matches == 2 && device.deviceClass != vr::TrackedDeviceClass::TrackedDeviceClass_HMD) ||
			(matches >= 1 && device.deviceClass == vr::TrackedDeviceClass::TrackedDeviceClass_HMD))) {
			return device.id;
		}
	}

	return -1;
}