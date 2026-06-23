#include "stdafx.h"
#include "Configuration.h"
#include "CalibrationChain.h"
#include "VRState.h"

#include <picojson.h>

#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>

static picojson::array FloatArray(const float *buf, size_t numFloats)
{
	picojson::array arr;

	for (int i = 0; i < numFloats; i++) {
		arr.push_back(picojson::value(double(buf[i])));
	}

	return arr;
}

static void LoadFloatArray(const picojson::value &obj, float *buf, size_t numFloats)
{
	if (!obj.is<picojson::array>()) {
		throw std::runtime_error("expected array, got " + obj.to_str());
	}

	auto &arr = obj.get<picojson::array>();
	if (arr.size() != numFloats) {
		throw std::runtime_error("wrong buffer size");
	}

	for (int i = 0; i < numFloats; i++) {
		buf[i] = (float) arr[i].get<double>();
	}
}

static void LoadStandby(StandbyDevice& device, picojson::value& value) {
	if (!value.is<picojson::object>()) {
		return;
	}
	auto& obj = value.get<picojson::object>();
	
	const auto &system = obj["tracking_system"];
	if (system.is<std::string>()) {
		device.trackingSystem = system.get<std::string>();
	}

	const auto& model = obj["model"];
	if (model.is<std::string>()) {
		device.model = model.get<std::string>();
	}

	const auto& serial = obj["serial"];
	if (serial.is<std::string>()) {
		device.serial = serial.get<std::string>();
	}
}

static void VisitAlignmentParams(CalibrationContext& ctx, std::function<void(const char *, double&)> MapParam) {
#define P(s) MapParam(#s, ctx.alignmentSpeedParams.s)
	P(align_speed_tiny);
	P(align_speed_small);
	P(align_speed_large);
	P(thr_trans_tiny);
	P(thr_trans_small);
	P(thr_trans_large);
	P(thr_rot_tiny);
	P(thr_rot_small);
	P(thr_rot_large);
	
	// Convert to double and back
	double tmp = ctx.continuousCalibrationThreshold;
	MapParam("continuousCalibrationThreshold", tmp);
	ctx.continuousCalibrationThreshold = (float)tmp;
}

static void LoadAlignmentParams(CalibrationContext& ctx, picojson::value& value) {
	ctx.ResetConfig();
	
	if (!value.is<picojson::object>()) {
		return;
	}
	auto& obj = value.get<picojson::object>();
	
	VisitAlignmentParams(ctx, [&](auto name, auto& param) {
		const picojson::value& node = obj[name];
		if (node.is<double>()) {
			param = (float)node.get<double>();
		}
	});
}

static picojson::object SaveAlignmentParams(CalibrationContext& ctx) {
	picojson::object obj;

	VisitAlignmentParams(ctx, [&](auto name, auto& param) {
		obj[name].set<double>(param);
	});

	return obj;
}

static void LoadRelativeTransform(picojson::object& relTransform, Eigen::AffineCompact3d& outPose) {
	Eigen::Vector3d refToTragetRoation;
	Eigen::Vector3d refToTargetTranslation;

	refToTragetRoation(0) = relTransform["roll"].get<double>();
	refToTragetRoation(1) = relTransform["yaw"].get<double>();
	refToTragetRoation(2) = relTransform["pitch"].get<double>();
	refToTargetTranslation(0) = relTransform["x"].get<double>();
	refToTargetTranslation(1) = relTransform["y"].get<double>();
	refToTargetTranslation(2) = relTransform["z"].get<double>();

	Eigen::Quaterniond rotationQuat =
		Eigen::AngleAxisd(refToTragetRoation[0], Eigen::Vector3d::UnitX()) *
		Eigen::AngleAxisd(refToTragetRoation[1], Eigen::Vector3d::UnitY()) *
		Eigen::AngleAxisd(refToTragetRoation[2], Eigen::Vector3d::UnitZ());
	Eigen::Matrix3d rotationMatrix = rotationQuat.toRotationMatrix();

	outPose = Eigen::AffineCompact3d::Identity();
	outPose.linear() = rotationMatrix;
	outPose.translation() = refToTargetTranslation;
}

static void ParseChainObject(const picojson::object& obj, CalibrationChain& chain, CalibrationContext& sharedCtx, bool isPrimary) {
	if (obj.count("chain_name") && obj.at("chain_name").is<std::string>()) {
		chain.name = obj.at("chain_name").get<std::string>();
	}

	chain.referenceTrackingSystem = obj.at("reference_tracking_system").get<std::string>();
	chain.targetTrackingSystem = obj.at("target_tracking_system").get<std::string>();
	chain.calibratedRotation(0) = obj.at("roll").get<double>();
	chain.calibratedRotation(1) = obj.at("yaw").get<double>();
	chain.calibratedRotation(2) = obj.at("pitch").get<double>();
	chain.calibratedTranslation(0) = obj.at("x").get<double>();
	chain.calibratedTranslation(1) = obj.at("y").get<double>();
	chain.calibratedTranslation(2) = obj.at("z").get<double>();
	picojson::value refDev = obj.at("reference_device");
	picojson::value tgtDev = obj.at("target_device");
	LoadStandby(chain.referenceStandby, refDev);
	LoadStandby(chain.targetStandby, tgtDev);

	chain.autostartContinuous = obj.at("autostart_continuous_calibration").evaluate_as_boolean();
	chain.continuousCalibrationOffset(0) = obj.at("continuous_calibration_target_offset_x").get<double>();
	chain.continuousCalibrationOffset(1) = obj.at("continuous_calibration_target_offset_y").get<double>();
	chain.continuousCalibrationOffset(2) = obj.at("continuous_calibration_target_offset_z").get<double>();

	if (obj.count("scale") && obj.at("scale").is<double>()) {
		chain.calibratedScale = obj.at("scale").get<double>();
	} else {
		chain.calibratedScale = 1.0;
	}

	if (obj.count("relative_pos_calibrated") && obj.at("relative_pos_calibrated").is<bool>()) {
		chain.relativePosCalibrated = obj.at("relative_pos_calibrated").get<bool>();
	}
	if (obj.count("lock_relative_position") && obj.at("lock_relative_position").is<bool>()) {
		chain.lockRelativePosition = obj.at("lock_relative_position").get<bool>();
	}
	if (obj.count("relative_transform") && obj.at("relative_transform").is<picojson::object>()) {
		auto relTransform = obj.at("relative_transform").get<picojson::object>();
		LoadRelativeTransform(relTransform, chain.refToTargetPose);
	}

	chain.slamReference = VRState::IsSlamTrackingSystem(chain.referenceTrackingSystem);
	chain.valid = true;
	// One-shot profiles saved before relative-gate fix stored relative_pos_calibrated=false
	// while lock_relative_position=true, which blocked ScanAndApplyProfile on load.
	if (chain.lockRelativePosition && !chain.relativePosCalibrated) {
		chain.relativePosCalibrated = true;
	}
	chain.calibration.setRelativeTransformation(chain.refToTargetPose, chain.relativePosCalibrated);
	chain.calibration.lockRelativePosition = chain.lockRelativePosition;
	chain.calibration.SeedEstimatedTransformation(
		CalibrationCalc::AffineFromSavedProfile(
			chain.calibratedTranslation, chain.calibratedRotation));

	if (isPrimary) {
		if (obj.count("alignment_params")) {
			picojson::value alignmentParams = obj.at("alignment_params");
			LoadAlignmentParams(sharedCtx, alignmentParams);
		}
		if (chain.autostartContinuous) {
			sharedCtx.state = CalibrationState::ContinuousStandby;
		}
		sharedCtx.quashTargetInContinuous = obj.at("quash_target_in_continuous").evaluate_as_boolean();
		sharedCtx.requireTriggerPressToApply = obj.at("require_trigger_press_to_apply").evaluate_as_boolean();
		sharedCtx.ignoreOutliers = obj.at("ignore_outliers").evaluate_as_boolean();
		if (obj.count("static_calibration") && obj.at("static_calibration").is<bool>()) {
			sharedCtx.enableStaticRecalibration = obj.at("static_calibration").get<bool>();
		}
		if (obj.count("jitter_threshold") && obj.at("jitter_threshold").is<double>()) {
			sharedCtx.jitterThreshold = (float)obj.at("jitter_threshold").get<double>();
		}
		if (obj.count("max_relative_error_threshold") && obj.at("max_relative_error_threshold").is<double>()) {
			sharedCtx.maxRelativeErrorThreshold = (float)obj.at("max_relative_error_threshold").get<double>();
		}
		if (obj.count("calibration_speed") && obj.at("calibration_speed").is<double>()) {
			sharedCtx.calibrationSpeed = (CalibrationContext::Speed)(int)obj.at("calibration_speed").get<double>();
		}
		if (obj.count("continuous_spike_threshold") && obj.at("continuous_spike_threshold").is<double>()) {
			sharedCtx.continuousSpikeThresholdM = (float)obj.at("continuous_spike_threshold").get<double>();
		}
		if (obj.count("continuous_frozen_frame_threshold") && obj.at("continuous_frozen_frame_threshold").is<double>()) {
			sharedCtx.continuousFrozenFrameThreshold = (int)obj.at("continuous_frozen_frame_threshold").get<double>();
		}
		if (obj.count("pause_on_reference_jitter") && obj.at("pause_on_reference_jitter").is<bool>()) {
			sharedCtx.pauseOnReferenceJitter = obj.at("pause_on_reference_jitter").get<bool>();
		}
		if (obj.count("apply_head_model_to_reference") && obj.at("apply_head_model_to_reference").is<bool>()) {
			sharedCtx.applyHeadModelToReference = obj.at("apply_head_model_to_reference").get<bool>();
		}
		if (obj.count("reject_yaw_drift_poses") && obj.at("reject_yaw_drift_poses").is<bool>()) {
			sharedCtx.rejectYawDriftPoses = obj.at("reject_yaw_drift_poses").get<bool>();
		}
		if (obj.count("trust_target_yaw") && obj.at("trust_target_yaw").is<bool>()) {
			sharedCtx.trustTargetYaw = obj.at("trust_target_yaw").get<bool>();
		}
		if (obj.count("compensate_pose_time_offset") && obj.at("compensate_pose_time_offset").is<bool>()) {
			sharedCtx.compensatePoseTimeOffset = obj.at("compensate_pose_time_offset").get<bool>();
		}
		if (obj.count("max_reference_pose_time_offset") && obj.at("max_reference_pose_time_offset").is<double>()) {
			sharedCtx.maxReferencePoseTimeOffset = (float)obj.at("max_reference_pose_time_offset").get<double>();
		}
		if (obj.count("max_pose_time_skew") && obj.at("max_pose_time_skew").is<double>()) {
			sharedCtx.maxPoseTimeSkew = (float)obj.at("max_pose_time_skew").get<double>();
		}
		if (obj.count("guardian_drift_trans_threshold_m") && obj.at("guardian_drift_trans_threshold_m").is<double>()) {
			sharedCtx.guardianDriftTransThresholdM = (float)obj.at("guardian_drift_trans_threshold_m").get<double>();
		}
		if (obj.count("guardian_drift_yaw_threshold_deg") && obj.at("guardian_drift_yaw_threshold_deg").is<double>()) {
			sharedCtx.guardianDriftYawThresholdRad = (float)obj.at("guardian_drift_yaw_threshold_deg").get<double>() * EIGEN_PI / 180.0f;
		}
		if (obj.count("guardian_drift_confirm_checks") && obj.at("guardian_drift_confirm_checks").is<double>()) {
			sharedCtx.guardianDriftConfirmChecks = (int)obj.at("guardian_drift_confirm_checks").get<double>();
		}
		if (obj.count("guardian_drift_cooldown_frames") && obj.at("guardian_drift_cooldown_frames").is<double>()) {
			sharedCtx.guardianDriftCooldownFrames = (int)obj.at("guardian_drift_cooldown_frames").get<double>();
		}
		if (obj.count("auto_recal_on_guardian_drift") && obj.at("auto_recal_on_guardian_drift").is<bool>()) {
			sharedCtx.autoRecalOnGuardianDrift = obj.at("auto_recal_on_guardian_drift").get<bool>();
		}
		if (obj.count("chaperone") && obj.at("chaperone").is<picojson::object>()) {
			auto chaperone = obj.at("chaperone").get<picojson::object>();
			sharedCtx.chaperone.autoApply = chaperone["auto_apply"].get<bool>();
			LoadFloatArray(chaperone["play_space_size"], sharedCtx.chaperone.playSpaceSize.v, 2);
			LoadFloatArray(
				chaperone["standing_center"],
				(float*)sharedCtx.chaperone.standingCenter.m,
				sizeof(sharedCtx.chaperone.standingCenter.m) / sizeof(float)
			);
			if (chaperone["geometry"].is<picojson::array>()) {
				auto& geometry = chaperone["geometry"].get<picojson::array>();
				constexpr size_t kMaxChaperoneGeometryFloats = 65536;
				if (geometry.size() > 0 && geometry.size() <= kMaxChaperoneGeometryFloats) {
					sharedCtx.chaperone.geometry.resize(geometry.size() * sizeof(float) / sizeof(sharedCtx.chaperone.geometry[0]));
					LoadFloatArray(chaperone["geometry"], (float*)sharedCtx.chaperone.geometry.data(), geometry.size());
					sharedCtx.chaperone.valid = true;
				}
			}
		}
	}
}

static void ParseProfile(CalibrationContext &ctx, std::istream &stream)
{
	picojson::value v;
	std::string err = picojson::parse(v, stream);
	if (!err.empty()) {
		throw std::runtime_error(err);
	}

	auto arr = v.get<picojson::array>();
	if (arr.size() < 1) {
		throw std::runtime_error("no profiles in file");
	}

	CalChains.clear();
	for (size_t i = 0; i < arr.size(); i++) {
		if (!arr[i].is<picojson::object>()) continue;
		CalibrationChain chain;
		ParseChainObject(arr[i].get<picojson::object>(), chain, ctx, i == 0);
		CalChains.push_back(chain);
	}

	SyncPrimaryChainToCalCtx();
	ctx.validProfile = !CalChains.empty() && CalChains[0].valid;
}


static void WriteStandby(StandbyDevice& device, picojson::value& value) {
	auto obj = picojson::object();

	obj["tracking_system"].set<std::string>(device.trackingSystem);
	obj["model"].set<std::string>(device.model);
	obj["serial"].set<std::string>(device.serial);

	value.set<picojson::object>(obj);
}


static void SetJsonNumber(picojson::object& obj, const char* key, double value) {
	const double stored = value;
	obj[key].set<double>(stored);
}

static void SetJsonBool(picojson::object& obj, const char* key, bool value) {
	const bool stored = value;
	obj[key].set<bool>(stored);
}

static picojson::object WriteChainObject(const CalibrationChain& chain, CalibrationContext& ctx, bool isPrimary) {
	picojson::object profile;
	if (!chain.name.empty()) {
		profile["chain_name"].set<std::string>(chain.name);
	}
	profile["reference_tracking_system"].set<std::string>(chain.referenceTrackingSystem);
	profile["target_tracking_system"].set<std::string>(chain.targetTrackingSystem);
	SetJsonNumber(profile, "roll", chain.calibratedRotation(0));
	SetJsonNumber(profile, "yaw", chain.calibratedRotation(1));
	SetJsonNumber(profile, "pitch", chain.calibratedRotation(2));
	SetJsonNumber(profile, "x", chain.calibratedTranslation(0));
	SetJsonNumber(profile, "y", chain.calibratedTranslation(1));
	SetJsonNumber(profile, "z", chain.calibratedTranslation(2));
	SetJsonNumber(profile, "scale", chain.calibratedScale);
	WriteStandby(const_cast<StandbyDevice&>(chain.referenceStandby), profile["reference_device"]);
	WriteStandby(const_cast<StandbyDevice&>(chain.targetStandby), profile["target_device"]);
	SetJsonBool(profile, "autostart_continuous_calibration", chain.autostartContinuous || chain.continuousActive);
	SetJsonNumber(profile, "continuous_calibration_target_offset_x", chain.continuousCalibrationOffset(0));
	SetJsonNumber(profile, "continuous_calibration_target_offset_y", chain.continuousCalibrationOffset(1));
	SetJsonNumber(profile, "continuous_calibration_target_offset_z", chain.continuousCalibrationOffset(2));

	Eigen::Vector3d refToTragetRoation = chain.refToTargetPose.rotation().eulerAngles(0, 1, 2);
	Eigen::Vector3d refToTargetTranslation = chain.refToTargetPose.translation();
	picojson::object refToTarget;
	SetJsonNumber(refToTarget, "x", refToTargetTranslation(0));
	SetJsonNumber(refToTarget, "y", refToTargetTranslation(1));
	SetJsonNumber(refToTarget, "z", refToTargetTranslation(2));
	SetJsonNumber(refToTarget, "roll", refToTragetRoation(0));
	SetJsonNumber(refToTarget, "yaw", refToTragetRoation(1));
	SetJsonNumber(refToTarget, "pitch", refToTragetRoation(2));
	SetJsonBool(profile, "relative_pos_calibrated", chain.relativePosCalibrated);
	SetJsonBool(profile, "lock_relative_position", chain.lockRelativePosition);
	profile["relative_transform"].set<picojson::object>(refToTarget);

	if (isPrimary) {
		profile["alignment_params"].set<picojson::object>(SaveAlignmentParams(ctx));
		SetJsonBool(profile, "quash_target_in_continuous", ctx.quashTargetInContinuous);
		SetJsonBool(profile, "require_trigger_press_to_apply", ctx.requireTriggerPressToApply);
		SetJsonBool(profile, "ignore_outliers", ctx.ignoreOutliers);
		SetJsonBool(profile, "static_calibration", ctx.enableStaticRecalibration);
		SetJsonNumber(profile, "jitter_threshold", (double)ctx.jitterThreshold);
		SetJsonNumber(profile, "max_relative_error_threshold", (double)ctx.maxRelativeErrorThreshold);
		SetJsonNumber(profile, "calibration_speed", (double)ctx.calibrationSpeed);
		SetJsonNumber(profile, "continuous_spike_threshold", (double)ctx.continuousSpikeThresholdM);
		SetJsonNumber(profile, "continuous_frozen_frame_threshold", (double)ctx.continuousFrozenFrameThreshold);
		SetJsonBool(profile, "pause_on_reference_jitter", ctx.pauseOnReferenceJitter);
		SetJsonBool(profile, "apply_head_model_to_reference", ctx.applyHeadModelToReference);
		SetJsonBool(profile, "reject_yaw_drift_poses", ctx.rejectYawDriftPoses);
		SetJsonBool(profile, "trust_target_yaw", ctx.trustTargetYaw);
		SetJsonBool(profile, "compensate_pose_time_offset", ctx.compensatePoseTimeOffset);
		SetJsonNumber(profile, "max_reference_pose_time_offset", (double)ctx.maxReferencePoseTimeOffset);
		SetJsonNumber(profile, "max_pose_time_skew", (double)ctx.maxPoseTimeSkew);
		SetJsonNumber(profile, "guardian_drift_trans_threshold_m", (double)ctx.guardianDriftTransThresholdM);
		SetJsonNumber(profile, "guardian_drift_yaw_threshold_deg", (double)(ctx.guardianDriftYawThresholdRad * 180.0 / EIGEN_PI));
		SetJsonNumber(profile, "guardian_drift_confirm_checks", (double)ctx.guardianDriftConfirmChecks);
		SetJsonNumber(profile, "guardian_drift_cooldown_frames", (double)ctx.guardianDriftCooldownFrames);
		SetJsonBool(profile, "auto_recal_on_guardian_drift", ctx.autoRecalOnGuardianDrift);
		if (ctx.chaperone.valid) {
			picojson::object chaperone;
			SetJsonBool(chaperone, "auto_apply", ctx.chaperone.autoApply);
			chaperone["play_space_size"].set<picojson::array>(FloatArray(ctx.chaperone.playSpaceSize.v, 2));
			chaperone["standing_center"].set<picojson::array>(FloatArray(
				(float*)ctx.chaperone.standingCenter.m,
				sizeof(ctx.chaperone.standingCenter.m) / sizeof(float)
			));
			chaperone["geometry"].set<picojson::array>(FloatArray(
				(float*)ctx.chaperone.geometry.data(),
				sizeof(ctx.chaperone.geometry[0]) / sizeof(float) * ctx.chaperone.geometry.size()
			));
			profile["chaperone"].set<picojson::object>(chaperone);
		}
	} else {
		SetJsonBool(profile, "quash_target_in_continuous", false);
		SetJsonBool(profile, "require_trigger_press_to_apply", false);
		SetJsonBool(profile, "ignore_outliers", ctx.ignoreOutliers);
		SetJsonBool(profile, "pause_on_reference_jitter", false);
		SetJsonBool(profile, "apply_head_model_to_reference", false);
		SetJsonBool(profile, "reject_yaw_drift_poses", false);
		SetJsonBool(profile, "trust_target_yaw", false);
		SetJsonBool(profile, "compensate_pose_time_offset", false);
		SetJsonBool(profile, "auto_recal_on_guardian_drift", false);
	}

	return profile;
}

static void WriteProfile(CalibrationContext &ctx, std::ostream &out)
{
	if (!ctx.validProfile) {
		return;
	}

	SyncCalCtxToPrimaryChain();
	EnsureDefaultChain();

	picojson::array profiles;
	for (size_t i = 0; i < CalChains.size(); i++) {
		picojson::value profileV;
		profileV.set<picojson::object>(WriteChainObject(CalChains[i], ctx, i == 0));
		profiles.push_back(profileV);
	}

	picojson::value profilesV;
	profilesV.set<picojson::array>(profiles);
	out << profilesV.serialize(true);
}

static void LogRegistryResult(LSTATUS result)
{
	char *message = nullptr;
	const DWORD size = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr, result, LANG_USER_DEFAULT, (LPSTR)&message, 0, nullptr);
	if (size > 0 && message != nullptr) {
		std::cerr << "Opening registry key: " << message << std::endl;
		LocalFree(message);
	}
}

static const char *RegistryKey = "Software\\OpenVR-SpaceCalibrator";

static std::string ReadRegistryKey()
{
	DWORD size = 0;
	auto result = RegGetValueA(HKEY_CURRENT_USER_LOCAL_SETTINGS, RegistryKey, "Config", RRF_RT_REG_SZ, 0, 0, &size);
	if (result != ERROR_SUCCESS) {
		LogRegistryResult(result);
		return "";
	}

	constexpr DWORD kMaxRegistryConfigBytes = 4 * 1024 * 1024;
	if (size == 0 || size > kMaxRegistryConfigBytes) {
		std::cerr << "Registry config size out of range: " << size << std::endl;
		return "";
	}

	std::string str;
	str.resize(size);

	result = RegGetValueA(HKEY_CURRENT_USER_LOCAL_SETTINGS, RegistryKey, "Config", RRF_RT_REG_SZ, 0, &str[0], &size);
	if (result != ERROR_SUCCESS) {
		LogRegistryResult(result);
		return "";
	}
	
	str.resize(size - 1);
	return str;
}

static void WriteRegistryKey(std::string str)
{
	HKEY hkey;
	auto result = RegCreateKeyExA(HKEY_CURRENT_USER_LOCAL_SETTINGS, RegistryKey, 0, REG_NONE, 0, KEY_ALL_ACCESS, 0, &hkey, 0);
	if (result != ERROR_SUCCESS) {
		LogRegistryResult(result);
		return;
	}

	DWORD size = static_cast<DWORD>(str.size() + 1);

	result = RegSetValueExA(hkey, "Config", 0, REG_SZ, reinterpret_cast<const BYTE*>(str.c_str()), size);
	if (result != ERROR_SUCCESS) {
		LogRegistryResult(result);
	}

	RegCloseKey(hkey);
}

void LoadProfile(CalibrationContext &ctx)
{
	// @TODO: Rewrite this to migrate configs from the registry to the spacecal directory
	//        I don't know why whoever wrote this thought writing to the registry in the 2020s was a good idea...
	//        NOTE: HKEY_CURRENT_USER_LOCAL_SETTINGS evaluates to	HKCU\Software\Classes\Local Settings
	//              Settings are currently stored at				HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator

	ctx.validProfile = false;

	auto str = ReadRegistryKey();
	if (str == "") {
		std::cout << "Profile is empty" << std::endl;
		ctx.Clear();
		return;
	}

	try {
		std::stringstream io(str);
		ParseProfile(ctx, io);
		std::cout << "Loaded profile" << std::endl;
		const bool migratedModes = ctx.lockRelativePosition && ctx.enableStaticRecalibration;
		ctx.ReconcileContinuousRefinementModes();
		if (ctx.validProfile && (migratedModes || (ctx.lockRelativePosition && ctx.relativePosCalibrated))) {
			SaveProfile(ctx);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << "Error loading profile: " << e.what() << std::endl;
	}
}

void SaveProfile(CalibrationContext &ctx)
{
	std::cout << "Saving profile to registry" << std::endl;

	std::stringstream io;
	WriteProfile(ctx, io);
	WriteRegistryKey(io.str());
}
