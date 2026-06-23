#include "stdafx.h"
#include "UserInterface.h"
#include "Calibration.h"
#include "Configuration.h"
#include "VRState.h"
#include "CalibrationMetrics.h"
#include "Version.h"

#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <imgui/imgui.h>
#include "imgui_extensions.h"
#include "ui_theme.h"
#include "DevicePresets.h"

void TextWithWidth(const char *label, const char *text, float width);
void DrawVectorElement(const std::string id, const char* text, double* value, int defaultValue = 0, const char* defaultValueStr = " 0 ");

VRState LoadVRState();
void BuildSystemSelection(const VRState &state);
void BuildDeviceSelections(const VRState &state);
void BuildProfileEditor();
void BuildMenu(bool runningInOverlay);

static const ImGuiWindowFlags bareWindowFlags =
	ImGuiWindowFlags_NoTitleBar |
	ImGuiWindowFlags_NoResize |
	ImGuiWindowFlags_NoMove |
	ImGuiWindowFlags_NoScrollbar |
	ImGuiWindowFlags_NoScrollWithMouse |
	ImGuiWindowFlags_NoCollapse;

void BuildContinuousCalDisplay();
void ShowVersionLine();

static bool runningInOverlay;

void BuildMainWindow(bool runningInOverlay_)
{
	runningInOverlay = runningInOverlay_;
	bool continuousCalibration = CalCtx.state == CalibrationState::Continuous || CalCtx.state == CalibrationState::ContinuousStandby;

	auto& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

	if (!ImGui::Begin("SpaceCalibrator", nullptr, bareWindowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_Button));

	if (continuousCalibration) {
		BuildContinuousCalDisplay();
	}
	else {
		auto state = LoadVRState();

		ImGui::BeginDisabled(CalCtx.state == CalibrationState::Continuous);
		BuildSystemSelection(state);
		BuildDeviceSelections(state);
		ImGui::EndDisabled();
		BuildMenu(runningInOverlay);
	}

	ShowVersionLine();

	ImGui::PopStyleColor();
	ImGui::End();
}

void ShowVersionLine() {
	ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()));
	if (!ImGui::BeginChild("bottom line", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_None)) {
		ImGui::EndChild();
		return;
	}
	ImGui::Text("Space Calibrator v" SPACECAL_VERSION_STRING);
	ImGui::SameLine();
	if (Driver.IsConnected()) {
		ImGui::TextColored(SpaceCalUI::StatusOk(), "| driver connected");
	} else {
		ImGui::TextColored(SpaceCalUI::StatusError(), "| driver offline");
	}
	if (runningInOverlay)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("| close VR overlay to use mouse");
	}
	ImGui::EndChild();
}

void CCal_BasicInfo();
void CCal_DrawSettings();
static void DrawDevicePresetSelector();

void BuildContinuousCalDisplay() {
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetWindowSize());
	ImGui::SetNextWindowBgAlpha(1);
	if (!ImGui::Begin("Continuous Calibration", nullptr,
		bareWindowFlags & ~ImGuiWindowFlags_NoTitleBar
	)) {
		ImGui::End();
		return;
	}

	ImVec2 contentRegion;
	contentRegion.x = ImGui::GetWindowContentRegionWidth();
	contentRegion.y = ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() * 2.1f;

	if (!ImGui::BeginChild("CCalDisplayFrame", contentRegion, ImGuiChildFlags_None)) {
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("CCalTabs", 0)) {
		if (ImGui::BeginTabItem("Status")) {
			CCal_BasicInfo();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("More Graphs")) {
			ShowCalibrationDebug(2, 3);
			ImGui::EndTabItem();
		}
		
		if (ImGui::BeginTabItem("Settings")) {
			CCal_DrawSettings();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();

	ShowVersionLine();

	ImGui::End();
}

static void ScaledDragFloat(const char* label, double& f, double scale, double min, double max, int flags = ImGuiSliderFlags_AlwaysClamp) {
	float v = (float) (f * scale);
	std::string labelStr = std::string(label);

	// If starts with ##, just do a normal SliderFloat
	if (labelStr.size() > 2 && labelStr[0] == '#' && labelStr[1] == '#') {
		ImGui::SliderFloat(label, &v, (float)min, (float)max, "%1.2f", flags);
	} else {
		// Otherwise do funny
		ImGui::Text(label);
		ImGui::SameLine();
		ImGui::PushID((std::string(label) + "_id").c_str());
		// Line up to a column, multiples of 100
		constexpr uint32_t LABEL_CURSOR = 100;
		uint32_t cursorPosX = (int) ImGui::GetCursorPosX();
		uint32_t roundedPosition = ((cursorPosX + LABEL_CURSOR / 2) / LABEL_CURSOR) * LABEL_CURSOR;
		ImGui::SetCursorPosX((float) roundedPosition);
		ImGui::SliderFloat((std::string("##") + label).c_str(), &v, (float)min, (float)max, "%1.2f", flags);
		ImGui::PopID();
	}
	
	f = v / scale;
}

void CCal_DrawSettings() {

	// panel size for boxes
	ImVec2 panel_size { ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x, 0 };

	ImGui::BeginGroupPanel("Device setup preset", panel_size);
	DrawDevicePresetSelector();
	ImGui::EndGroupPanel();

	ImGui::BeginGroupPanel("Tip", panel_size);
	ImGui::Text("Hover over settings to learn more about them!");
	ImGui::EndGroupPanel();


	// @TODO: Group in UI

	// Section: Alignment speeds
	{
		ImGui::BeginGroupPanel("Calibration speeds", panel_size);

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped(
			"SpaceCalibrator uses up to three different speeds at which it drags the calibration back into "
			"position when drift occurs. These settings control how far off the calibration should be before going back to low speed (for "
			"Decel) or going to higher speeds (for Slow and Fast)."
		);
		ImGui::PopStyleColor();

		// Calibration Speed
		{
			ImGui::BeginGroupPanel("Calibration speed", panel_size);
		
			auto speed = CalCtx.calibrationSpeed;

			ImGui::Columns(3, nullptr, false);
			if (ImGui::RadioButton(" Fast          ", speed == CalibrationContext::FAST)) {
				CalCtx.calibrationSpeed = CalibrationContext::FAST;
			}
			ImGui::NextColumn();
			if (ImGui::RadioButton(" Slow          ", speed == CalibrationContext::SLOW)) {
				CalCtx.calibrationSpeed = CalibrationContext::SLOW;
			}
			ImGui::NextColumn();
			if (ImGui::RadioButton(" Very Slow     ", speed == CalibrationContext::VERY_SLOW)) {
				CalCtx.calibrationSpeed = CalibrationContext::VERY_SLOW;
			}
			ImGui::Columns(1);

			ImGui::EndGroupPanel();
		}

		if (ImGui::BeginTable("SpeedThresholds", 3, 0)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Translation (mm)");
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("Rotation (degrees)");


			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Decel");
			ImGui::TableSetColumnIndex(1);
			ScaledDragFloat("##TransDecel", CalCtx.alignmentSpeedParams.thr_trans_tiny, 1000.0, 0, 20.0);
			ImGui::TableSetColumnIndex(2);
			ScaledDragFloat("##RotDecel", CalCtx.alignmentSpeedParams.thr_rot_tiny, 180.0 / EIGEN_PI, 0, 5.0);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Slow");
			ImGui::TableSetColumnIndex(1);
			ScaledDragFloat("##TransSlow", CalCtx.alignmentSpeedParams.thr_trans_small, 1000.0,
				CalCtx.alignmentSpeedParams.thr_trans_tiny * 1000.0, 20.0);
			ImGui::TableSetColumnIndex(2);
			ScaledDragFloat("##RotSlow", CalCtx.alignmentSpeedParams.thr_rot_small, 180.0 / EIGEN_PI,
				CalCtx.alignmentSpeedParams.thr_rot_tiny * (180.0 / EIGEN_PI), 10.0);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Fast");
			ImGui::TableSetColumnIndex(1);
			ScaledDragFloat("##TransFast", CalCtx.alignmentSpeedParams.thr_trans_large, 1000.0,
				CalCtx.alignmentSpeedParams.thr_trans_small * 1000.0, 50.0);
			ImGui::TableSetColumnIndex(2);
			ScaledDragFloat("##RotFast", CalCtx.alignmentSpeedParams.thr_rot_large, 180.0 / EIGEN_PI,
				CalCtx.alignmentSpeedParams.thr_rot_small * (180.0 / EIGEN_PI), 20.0);

			ImGui::EndTable();
		}

		ImGui::EndGroupPanel();
	}

	// Section: Alignment speeds
	{
		ImGui::BeginGroupPanel("Alignment speeds", panel_size);

		// ImGui::Separator();
		// ImGui::Text("Alignment speeds");
		ScaledDragFloat("Decel", CalCtx.alignmentSpeedParams.align_speed_tiny, 1.0, 0, 2.0, 0);
		ScaledDragFloat("Slow", CalCtx.alignmentSpeedParams.align_speed_small, 1.0, 0, 2.0, 0);
		ScaledDragFloat("Fast", CalCtx.alignmentSpeedParams.align_speed_large, 1.0, 0, 2.0, 0);
		
		ImGui::EndGroupPanel();
	}
	

	// Section: Continuous Calibration settings
	{
		ImGui::BeginGroupPanel("Continuous calibration", panel_size);
		{
			// @TODO: Reduce code duplication (tooltips)
			// Recalibration threshold
			ImGui::Text("Recalibration threshold");
			ImGui::SameLine();
			ImGui::PushID("recalibration_threshold");
			ImGui::SliderFloat("##recalibration_threshold_slider", &CalCtx.continuousCalibrationThreshold, 1.01f, 10.0f, "%1.1f", 0);
			if (ImGui::IsItemHovered(0)) {
				ImGui::SetTooltip("Controls how good the calibration must be before realigning the trackers.\n"
					"Higher values cause calibration to happen less often, and may be useful for systems with lots of tracking drift.");
			}
			ImGui::PopID();

			// Recalibration threshold
			ImGui::Text("Max relative error threshold");
			ImGui::SameLine();
			ImGui::PushID("max_relative_error_threshold");
			ImGui::SliderFloat("##max_relative_error_threshold_slider", &CalCtx.maxRelativeErrorThreshold, 0.01f, 1.0f, "%1.1f", 0);
			if (ImGui::IsItemHovered(0)) {
				ImGui::SetTooltip("Controls the maximum acceptable relative error. If the error from the relative calibration is too poor, the calibration will be discarded.");
			}
			ImGui::PopID();

			// Jitter threshold
			ImGui::Text("Jitter threshold");
			ImGui::SameLine();
			ImGui::PushID("jtter_threshold");
			ImGui::SliderFloat("##jitter_threshold_slider", &CalCtx.jitterThreshold, 0.1f, 10.0f, "%1.1f", 0);
			if (ImGui::IsItemHovered(0)) {
				ImGui::SetTooltip("Controls how much jitter will be allowed for calibration.\n"
					"Higher values allow worse tracking to calibrate, but may result in poorer tracking.");
			}
			ImGui::PopID();

			// Quest SLAM / Guardian settings (fork additions for Quest SLAM + lighthouse)
			{
				ImVec2 panel_size_inner { panel_size.x - 11 * 2, 0};
				ImGui::BeginGroupPanel("Quest SLAM / Guardian settings", panel_size_inner);

				ImGui::Checkbox("Chaperone auto apply (stored)", &CalCtx.chaperone.autoApply);
				ImGui::SameLine();

				// Spike threshold
				ImGui::Text("Spike threshold (m)");
				ImGui::SameLine();
				ImGui::PushID("spike_threshold");
				ImGui::SliderFloat("##spike_threshold_slider", &CalCtx.continuousSpikeThresholdM, 0.01f, 0.2f, "%.3f", 0);
				if (ImGui::IsItemHovered(0)) {
					ImGui::SetTooltip("Base spike rejection threshold for continuous samples.\nHigher for jittery SLAM refs; auto-scaled by jitter.");
				}
				ImGui::PopID();

				// Frozen threshold
				ImGui::Text("Frozen frame threshold");
				ImGui::SameLine();
				ImGui::PushID("frozen_threshold");
				int frozen = CalCtx.continuousFrozenFrameThreshold;
				if (ImGui::SliderInt("##frozen_slider", &frozen, 1, 20)) {
					CalCtx.continuousFrozenFrameThreshold = frozen;
				}
				if (ImGui::IsItemHovered(0)) {
					ImGui::SetTooltip("Frames with no movement before considering ref frozen (skips in cont cal for SLAM).");
				}
				ImGui::PopID();

				// Checkboxes row 1
				ImGui::Checkbox("Pause on ref jitter", &CalCtx.pauseOnReferenceJitter);
				ImGui::SameLine();
				ImGui::Checkbox("Apply head model to ref", &CalCtx.applyHeadModelToReference);
				ImGui::Checkbox("Reject yaw drift poses", &CalCtx.rejectYawDriftPoses);
				ImGui::SameLine();
				ImGui::Checkbox("Trust target yaw", &CalCtx.trustTargetYaw);

				// Checkboxes row 2
				ImGui::Checkbox("Compensate pose time offset", &CalCtx.compensatePoseTimeOffset);
				ImGui::SameLine();
				ImGui::Checkbox("Auto recal on guardian drift", &CalCtx.autoRecalOnGuardianDrift);

				// Time params
				ImGui::Text("Max ref pose time offset (s)");
				ImGui::SameLine();
				ImGui::PushID("max_ref_time");
				ImGui::SliderFloat("##max_ref_time_slider", &CalCtx.maxReferencePoseTimeOffset, 0.01f, 0.2f, "%.3f", 0);
				ImGui::PopID();

				ImGui::Text("Max pose time skew (s)");
				ImGui::SameLine();
				ImGui::PushID("max_skew");
				ImGui::SliderFloat("##max_skew_slider", &CalCtx.maxPoseTimeSkew, 0.01f, 0.2f, "%.3f", 0);
				if (ImGui::IsItemHovered(0)) {
					ImGui::SetTooltip("Relaxed in cont cal for SLAM+VD timestamp skew between ref and target.");
				}
				ImGui::PopID();

				// Guardian drift sub
				ImGui::Text("Guardian drift trans thresh (m)");
				ImGui::SameLine();
				ImGui::PushID("g_trans");
				ImGui::SliderFloat("##g_trans_slider", &CalCtx.guardianDriftTransThresholdM, 0.01f, 0.1f, "%.3f", 0);
				ImGui::PopID();

				ImGui::Text("Guardian drift yaw thresh (deg)");
				ImGui::SameLine();
				ImGui::PushID("g_yaw");
				float yawDeg = CalCtx.guardianDriftYawThresholdRad * 180.0f / EIGEN_PI;
				if (ImGui::SliderFloat("##g_yaw_slider", &yawDeg, 1.0f, 20.0f, "%.1f", 0)) {
					CalCtx.guardianDriftYawThresholdRad = yawDeg * EIGEN_PI / 180.0f;
				}
				ImGui::PopID();

				ImGui::Text("Guardian confirm checks");
				ImGui::SameLine();
				ImGui::PushID("g_confirm");
				int confirms = CalCtx.guardianDriftConfirmChecks;
				if (ImGui::SliderInt("##g_confirm_slider", &confirms, 1, 10)) {
					CalCtx.guardianDriftConfirmChecks = confirms;
				}
				ImGui::PopID();

				ImGui::Text("Guardian cooldown frames");
				ImGui::SameLine();
				ImGui::PushID("g_cooldown");
				int cooldown = CalCtx.guardianDriftCooldownFrames;
				if (ImGui::SliderInt("##g_cooldown_slider", &cooldown, 10, 120)) {
					CalCtx.guardianDriftCooldownFrames = cooldown;
				}
				ImGui::PopID();

				ImGui::EndGroupPanel();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped("Controls how often SpaceCalibrator synchronises playspaces.");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered(0)) {
				ImGui::SetTooltip("Controls how good the calibration must be before realigning the trackers.\n"
					"Higher values cause calibration to happen less often, and may be useful for system with lots of tracking drift.");
			}
		}

		{
			// Tracker offset
			// ImVec2 panel_size_inner { ImGui::GetCurrentWindow()->DC.ItemWidth, 0};
			ImVec2 panel_size_inner { panel_size.x - 11 * 2, 0};
			ImGui::BeginGroupPanel("Tracker offset", panel_size_inner);
			DrawVectorElement("cc_tracker_offset", "X", &CalCtx.continuousCalibrationOffset.x());
			DrawVectorElement("cc_tracker_offset", "Y", &CalCtx.continuousCalibrationOffset.y());
			DrawVectorElement("cc_tracker_offset", "Z", &CalCtx.continuousCalibrationOffset.z());
			ImGui::EndGroupPanel();
		}

		{
			// Playspace offset
			ImVec2 panel_size_inner{ panel_size.x - 11 * 2, 0 };
			ImGui::BeginGroupPanel("Playspace scale", panel_size_inner);
			DrawVectorElement("cc_playspace_scale", "Playspace Scale", &CalCtx.calibratedScale, 1, " 1 ");
			ImGui::EndGroupPanel();
		}

		ImGui::EndGroupPanel();
	}

	ImGui::NewLine();
	ImGui::Indent();
	if (ImGui::Button("Reset settings")) {
		CalCtx.ResetConfig();
	}
	ImGui::Unindent();
	ImGui::NewLine();

	// Section: Contributors credits
	{
		ImGui::BeginGroupPanel("Credits", panel_size);

		ImGui::TextDisabled("tach");
		ImGui::TextDisabled("pushrax");
		ImGui::TextDisabled("bd_");
		ImGui::TextDisabled("ArcticFox");
		ImGui::TextDisabled("hekky");
		ImGui::TextDisabled("pimaker");

		ImGui::EndGroupPanel();
	}
}

void DrawVectorElement(const std::string id, const char* text, double* value, int defaultValue, const char* defaultValueStr) {
	constexpr float CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA = 0.01f;

	ImGui::Text(text);

	ImGui::SameLine();

	ImGui::PushID((id + text + "_btn_reset").c_str());
	if (ImGui::Button(defaultValueStr)) {
		*value *= defaultValue;
	}
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::ArrowButton((id + text + "_decrease").c_str(), ImGuiDir_Down)) {
		*value -= CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA;
	}
	ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::PushID((id + text + "_text_field").c_str());
	ImGui::InputDouble("##label", value, 0, 0, "%.2f");
	ImGui::PopID();
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::ArrowButton((id + text + "_increase").c_str(), ImGuiDir_Up)) {
		*value += CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA;
	}
}

inline const char* GetPrettyTrackingSystemName(const std::string& value) {
	// To comply with SteamVR branding guidelines (page 29), we rename devices under lighthouse tracking to SteamVR Tracking.
	if (value == "lighthouse" || value == "aapvr") {
		return "SteamVR Tracking";
	}
	return value.c_str();
}

static void SetStatusCellBg(const ImVec4& color) {
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, SpaceCalUI::ColorToU32(color, 0.35f));
}

static void DrawDevicePresetSelector() {
	const DevicePresetDefinition* current = FindDevicePreset(CalCtx.devicePresetId.c_str());
	const char* preview = current ? current->label : "Custom (manual sliders)";

	ImGui::Text("Device setup");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##device_setup_preset", preview)) {
		for (int i = 0; i < DevicePresetCount(); ++i) {
			const DevicePresetDefinition* preset = FindDevicePresetByIndex(i);
			if (!preset) {
				continue;
			}
			const bool selected = CalCtx.devicePresetId == preset->id;
			if (ImGui::Selectable(preset->label, selected)) {
				if (!selected) {
					ApplyDevicePreset(CalCtx, preset->id);
					SaveProfile(CalCtx);
				}
			}
			if (ImGui::IsItemHovered() && preset->description[0] != '\0') {
				ImGui::SetTooltip("%s", preset->description);
			}
		}
		ImGui::EndCombo();
	}
	if (current && current->description[0] != '\0') {
		ImGui::TextDisabled("%s", current->description);
	}
	if (CalCtx.slamReference) {
		ImGui::TextDisabled(
			"Reference: %s | skew %.3fs | offset %.3fs",
			GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem),
			CalCtx.maxPoseTimeSkew,
			CalCtx.maxReferencePoseTimeOffset);
	}
	ImGui::Spacing();
}

static void DrawProfileStatusBanner() {
	if (!Driver.IsConnected()) {
		ImGui::TextColored(SpaceCalUI::StatusError(),
			"Driver not connected — restart SteamVR after running deploy.ps1");
		ImGui::Spacing();
		return;
	}

	if (!CalCtx.validProfile) {
		ImGui::TextColored(SpaceCalUI::StatusIdle(),
			"No saved profile — run one-shot calibration to merge playspaces");
		ImGui::Spacing();
		return;
	}

	if (CalCtx.enabled) {
		ImGui::TextColored(SpaceCalUI::StatusOk(),
			"Offsets active: %s aligned to %s",
			GetPrettyTrackingSystemName(CalCtx.targetTrackingSystem),
			GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
	} else if (CalCtx.referenceID < 0) {
		ImGui::TextColored(SpaceCalUI::StatusWarn(),
			"Profile saved — reference device (%s) not detected",
			GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
	} else {
		ImGui::TextColored(SpaceCalUI::StatusWarn(),
			"Profile saved — HMD tracking system mismatch (expected %s)",
			GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
	}

	if (CalCtx.slamReference) {
		ImGui::TextDisabled("Quest SLAM reference — VD latency preset applies during calibration");
	}
	if (!CalCtx.steamVrPathOk && !CalCtx.steamVrPathWarning.empty()) {
		ImGui::TextColored(SpaceCalUI::StatusWarn(), "%s", CalCtx.steamVrPathWarning.c_str());
		ImGui::TextDisabled("VD Settings: OpenXR runtime -> SteamVR (not VDXR)");
	}

	ImGui::Spacing();
}

void CCal_BasicInfo() {
	DrawDevicePresetSelector();

	if (CalCtx.enabled) {
		ImGui::TextColored(SpaceCalUI::StatusOk(), "Offsets active");
	} else if (CalCtx.validProfile) {
		ImGui::TextColored(SpaceCalUI::StatusWarn(), "Saved profile loaded — waiting to apply");
	}
	if (CalCtx.lastLiveErrorCurrentCalMm > 0.f || CalCtx.lastLiveErrorByRelPoseMm > 0.f) {
		const ImVec4 errColor = CalCtx.lastLiveErrorCurrentCalMm > 50.f
			? SpaceCalUI::StatusWarn()
			: SpaceCalUI::StatusIdle();
		ImGui::TextColored(errColor, "Live error: applied %.0f mm | rel-pose %.0f mm",
			CalCtx.lastLiveErrorCurrentCalMm, CalCtx.lastLiveErrorByRelPoseMm);
		if (CalCtx.lastLiveErrorCurrentCalMm > 25.f && CalCtx.lockRelativePosition) {
			ImGui::TextDisabled("High applied drift — lock-relative or diverged recovery will attempt a bounded correction");
		}
	}
	ImGui::Spacing();

	if (ImGui::BeginTable("DeviceInfo", 2, 0)) {
		ImGui::TableSetupColumn("Reference device");
		ImGui::TableSetupColumn("Target device");
		ImGui::TableHeadersRow();

		const char* refTrackingSystem = GetPrettyTrackingSystemName(CalCtx.referenceStandby.trackingSystem);
		const char* targetTrackingSystem = GetPrettyTrackingSystemName(CalCtx.targetStandby.trackingSystem);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginGroup();
		ImGui::Text("%s / %s / %s",
			refTrackingSystem,
			CalCtx.referenceStandby.model.c_str(),
			CalCtx.referenceStandby.serial.c_str()
		);
		const char* status;
		if (CalCtx.referenceID < 0) {
			SetStatusCellBg(SpaceCalUI::StatusError());
			status = "NOT FOUND";
		} else if (!CalCtx.ReferencePoseIsValidSimple()) {
			SetStatusCellBg(SpaceCalUI::StatusWarn());
			status = "NOT TRACKING";
		} else {
			status = "OK";
		}
		ImGui::Text("Status: %s", status);
		ImGui::EndGroup();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginGroup();
		ImGui::Text("%s / %s / %s",
			targetTrackingSystem,
			CalCtx.targetStandby.model.c_str(),
			CalCtx.targetStandby.serial.c_str()
		);
		if (CalCtx.targetID < 0) {
			SetStatusCellBg(SpaceCalUI::StatusError());
			status = "NOT FOUND";
		}
		else if (!CalCtx.TargetPoseIsValidSimple()) {
			SetStatusCellBg(SpaceCalUI::StatusWarn());
			status = "NOT TRACKING";
		}
		else {
			status = "OK";
		}
		ImGui::Text("Status: %s", status);
		ImGui::EndGroup();

		ImGui::EndTable();
	}

	float width = ImGui::GetWindowContentRegionWidth(), scale = 1.0f;

	if (ImGui::BeginTable("##CCal_Cancel", Metrics::enableLogs ? 3 : 2, 0, ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button("Cancel Continuous Calibration", ImVec2(-FLT_MIN, 0.0f))) {
			EndContinuousCalibration();
		}

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("Debug: Force break calibration", ImVec2(-FLT_MIN, 0.0f))) {
			DebugApplyRandomOffset();
		}

		if (Metrics::enableLogs) {
			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button("Debug: Mark logs", ImVec2(-FLT_MIN, 0.0f))) {
				Metrics::WriteLogAnnotation("MARK LOGS");
			}
		}

		ImGui::EndTable();
	}

	ImGui::Checkbox("Hide tracker", &CalCtx.quashTargetInContinuous);
	ImGui::SameLine();
	if (ImGui::Checkbox("Lock relative transform", &CalCtx.lockRelativePosition)) {
		if (CalCtx.lockRelativePosition) {
			CalCtx.enableStaticRecalibration = false;
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Quest/VD default: refine the saved one-shot profile using relative pose.\nMutually exclusive with static recalibration.");
	}
	ImGui::SameLine();
	if (CalCtx.lockRelativePosition) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Checkbox("Static recalibration", &CalCtx.enableStaticRecalibration)) {
		if (CalCtx.enableStaticRecalibration) {
			CalCtx.lockRelativePosition = false;
		}
	}
	if (CalCtx.lockRelativePosition) {
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("Turn off lock relative to use static recalibration instead.");
		}
	} else if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Alternative continuous refinement path when lock relative is off.");
	}
	ImGui::SameLine();
	ImGui::Checkbox("Enable debug logs", &Metrics::enableLogs);
	ImGui::SameLine();
	ImGui::Checkbox("Require triggers", &CalCtx.requireTriggerPressToApply);
	ImGui::Checkbox("Ignore outliers", &CalCtx.ignoreOutliers);

	// Status field...

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 1));

	for (const auto& msg : CalCtx.messages) {
		if (msg.type == CalibrationContext::Message::String) {
			ImGui::TextWrapped("> %s", msg.str.c_str());
		}
	}

	ImGui::PopStyleColor();

	ShowCalibrationDebug(1, 3);
}

void BuildMenu(bool runningInOverlay)
{
	auto &io = ImGui::GetIO();
	ImGuiStyle &style = ImGui::GetStyle();
	ImGui::Text("");

	if (CalCtx.state == CalibrationState::None)
	{
		ImGui::BeginGroupPanel("Device setup preset", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
		DrawDevicePresetSelector();
		ImGui::EndGroupPanel();

		DrawProfileStatusBanner();

		float width = ImGui::GetWindowContentRegionWidth(), scale = 1.0f;
		if (CalCtx.validProfile)
		{
			width -= style.FramePadding.x * 4.0f;
			scale = 1.0f / 4.0f;
		}

		if (SpaceCalUI::PrimaryButton("Start Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
		{
			ImGui::OpenPopup("Calibration Progress");
			StartCalibration();
		}

		ImGui::SameLine();
		if (!CalCtx.validProfile) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Continuous Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
			StartContinuousCalibration();
		}
		if (!CalCtx.validProfile) {
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Run one-shot calibration first to create a saved profile");
			}
		}

		if (CalCtx.validProfile)
		{
			ImGui::SameLine();
			if (ImGui::Button("Edit Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				CalCtx.state = CalibrationState::Editing;
			}

			ImGui::SameLine();
			if (SpaceCalUI::DangerButton("Clear Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				CalCtx.Clear();
				SaveProfile(CalCtx);
			}
		}

		width = ImGui::GetWindowContentRegionWidth();
		scale = 1.0f;
		if (CalCtx.chaperone.valid)
		{
			width -= style.FramePadding.x * 2.0f;
			scale = 0.5;
		}

		ImGui::Text("");
		if (ImGui::Button("Copy Chaperone Bounds to profile", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
		{
			LoadChaperoneBounds();
			SaveProfile(CalCtx);
		}

		if (CalCtx.chaperone.valid)
		{
			ImGui::SameLine();
			if (ImGui::Button("Paste Chaperone Bounds", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				ApplyChaperoneBounds();
			}

			if (ImGui::Checkbox(" Paste Chaperone Bounds automatically when geometry resets", &CalCtx.chaperone.autoApply))
			{
				SaveProfile(CalCtx);
			}
		}

		ImGui::Text("");
		auto speed = CalCtx.calibrationSpeed;

		ImGui::Columns(4, nullptr, false);
		ImGui::Text("Calibration Speed");

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Fast          ", speed == CalibrationContext::FAST))
			CalCtx.calibrationSpeed = CalibrationContext::FAST;

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Slow          ", speed == CalibrationContext::SLOW))
			CalCtx.calibrationSpeed = CalibrationContext::SLOW;

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Very Slow     ", speed == CalibrationContext::VERY_SLOW))
			CalCtx.calibrationSpeed = CalibrationContext::VERY_SLOW;

		ImGui::Columns(1);
	}
	else if (CalCtx.state == CalibrationState::Editing)
	{
		BuildProfileEditor();

		if (ImGui::Button("Save Profile", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2)))
		{
			SaveProfile(CalCtx);
			CalCtx.state = CalibrationState::None;
		}
	}
	else
	{
		ImGui::Button("Calibration in progress...", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2));
	}

	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 40.0f, io.DisplaySize.y - 40.0f), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("Calibration Progress", nullptr, bareWindowFlags))
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImVec4(0, 0, 0, 1));
		for (auto &message : CalCtx.messages)
		{
			switch (message.type)
			{
			case CalibrationContext::Message::String:
				ImGui::TextWrapped(message.str.c_str());
				break;
			case CalibrationContext::Message::Progress:
				float fraction = (float)message.progress / (float)message.target;
				ImGui::Text("");
				ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), "");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetFontSize() - style.FramePadding.y * 2);
				ImGui::Text(" %d%%", (int)(fraction * 100));
				break;
			}
		}
		ImGui::PopStyleColor();

		if (CalCtx.state == CalibrationState::None)
		{
			ImGui::Text("");
			if (ImGui::Button("Close", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2)))
				ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void BuildSystemSelection(const VRState &state)
{
	if (state.trackingSystems.empty())
	{
		ImGui::Text("No tracked devices are present");
		return;
	}

	ImGuiStyle &style = ImGui::GetStyle();
	float paneWidth = ImGui::GetWindowContentRegionWidth() / 2 - style.FramePadding.x;

	TextWithWidth("ReferenceSystemLabel", "Reference Space", paneWidth);
	ImGui::SameLine();
	TextWithWidth("TargetSystemLabel", "Target Space", paneWidth);

	int currentReferenceSystem = -1;
	int currentTargetSystem = -1;
	int firstReferenceSystemNotTargetSystem = -1;

	std::vector<const char *> referenceSystems;
	std::vector<const char *> referenceSystemsUi;
	for (const std::string& str : state.trackingSystems)
	{
		if (str == CalCtx.referenceTrackingSystem)
		{
			currentReferenceSystem = (int) referenceSystems.size();
		}
		else if (firstReferenceSystemNotTargetSystem == -1 && str != CalCtx.targetTrackingSystem)
		{
			firstReferenceSystemNotTargetSystem = (int) referenceSystems.size();
		}
		referenceSystems.push_back(str.c_str());
		referenceSystemsUi.push_back(GetPrettyTrackingSystemName(str));
	}

	if (currentReferenceSystem == -1 && CalCtx.referenceTrackingSystem == "")
	{
		if (CalCtx.state == CalibrationState::ContinuousStandby) {
			auto iter = std::find(state.trackingSystems.begin(), state.trackingSystems.end(), CalCtx.referenceStandby.trackingSystem);
			if (iter != state.trackingSystems.end()) {
				currentReferenceSystem = (int) (iter - state.trackingSystems.begin());
			}
		}
		else {
			currentReferenceSystem = firstReferenceSystemNotTargetSystem;
		}
	}

	ImGui::PushItemWidth(paneWidth);
	ImGui::Combo("##ReferenceTrackingSystem", &currentReferenceSystem, &referenceSystemsUi[0], (int)referenceSystemsUi.size());

	if (currentReferenceSystem != -1 && currentReferenceSystem < (int) referenceSystems.size())
	{
		CalCtx.referenceTrackingSystem = std::string(referenceSystems[currentReferenceSystem]);
		if (CalCtx.referenceTrackingSystem == CalCtx.targetTrackingSystem)
			CalCtx.targetTrackingSystem = "";
	}

	if (CalCtx.targetTrackingSystem == "") {
		if (CalCtx.state == CalibrationState::ContinuousStandby) {
			auto iter = std::find(state.trackingSystems.begin(), state.trackingSystems.end(), CalCtx.targetStandby.trackingSystem);
			if (iter != state.trackingSystems.end()) {
				currentTargetSystem = (int) (iter - state.trackingSystems.begin());
			}
		}
		else {
			currentTargetSystem = 0;
		}
	}

	std::vector<const char *> targetSystems;
	std::vector<const char *> targetSystemsUi;
	for (const std::string& str : state.trackingSystems)
	{
		if (str != CalCtx.referenceTrackingSystem)
		{
			if (str != "" && str == CalCtx.targetTrackingSystem)
				currentTargetSystem = (int) targetSystems.size();
			targetSystems.push_back(str.c_str());
			targetSystemsUi.push_back(GetPrettyTrackingSystemName(str));
		}
	}

	ImGui::SameLine();
	ImGui::Combo("##TargetTrackingSystem", &currentTargetSystem, &targetSystemsUi[0], (int)targetSystemsUi.size());

	if (currentTargetSystem != -1 && currentTargetSystem < targetSystems.size())
	{
		CalCtx.targetTrackingSystem = std::string(targetSystems[currentTargetSystem]);
	}

	ImGui::PopItemWidth();
}

void AppendSeparated(std::string &buffer, const std::string &suffix)
{
	if (!buffer.empty())
		buffer += " | ";
	buffer += suffix;
}

std::string LabelString(const VRDevice &device)
{
	std::string label;

	/*if (device.controllerRole == vr::TrackedControllerRole_LeftHand)
		label = "Left Controller";
	else if (device.controllerRole == vr::TrackedControllerRole_RightHand)
		label = "Right Controller";
	else if (device.deviceClass == vr::TrackedDeviceClass_Controller)
		label = "Controller";
	else if (device.deviceClass == vr::TrackedDeviceClass_HMD)
		label = "HMD";
	else if (device.deviceClass == vr::TrackedDeviceClass_GenericTracker)
		label = "Tracker";*/

	AppendSeparated(label, device.model);
	AppendSeparated(label, device.serial);
	return label;
}

std::string LabelString(const StandbyDevice& device) {
	std::string label("< ");

	label += device.model;
	AppendSeparated(label, device.serial);

	label += " >";
	return label;
}

void BuildDeviceSelection(
	const VRState &state,
	int &initialSelected,
	const std::string &system,
	StandbyDevice &standbyDevice,
	bool preferHmdForAutoSelect = false)
{
	int selected = initialSelected;
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Devices from: %s", GetPrettyTrackingSystemName(system));

	if (selected != -1)
	{
		bool matched = false;
		for (auto &device : state.devices)
		{
			if (device.trackingSystem != system)
				continue;

			if (selected == device.id)
			{
				matched = true;
				break;
			}
		}

		if (!matched)
		{
			// Device is no longer present.
			selected = -1;
		}
	}

	bool standby = CalCtx.state == CalibrationState::ContinuousStandby;

	if (selected == -1 && !standby)
	{
		if (preferHmdForAutoSelect) {
			for (const auto& device : state.devices) {
				if (device.trackingSystem != system) {
					continue;
				}
				if (device.deviceClass == vr::TrackedDeviceClass_HMD) {
					selected = device.id;
					break;
				}
			}
		}

		if (selected == -1) {
			for (auto &device : state.devices)
			{
				if (device.trackingSystem != system)
					continue;

				if (device.controllerRole == vr::TrackedControllerRole_LeftHand)
				{
					selected = device.id;
					break;
				}
			}
		}

		if (selected == -1) {
			for (auto& device : state.devices)
			{
				if (device.trackingSystem != system)
					continue;
				
				selected = device.id;
				break;
			}
		}
	}

	uint64_t iterator = 0;
	if (selected == -1 && standby) {
		bool present = false;
		for (auto& device : state.devices)
		{
			if (device.trackingSystem != system)
				continue;

			if (standbyDevice.model != device.model) continue;
			if (standbyDevice.serial != device.serial) continue;

			present = true;
			break;
		}

		if (!present) {
			auto label = LabelString(standbyDevice);
			std::string uniqueId = label + "_pass0_" + std::to_string(iterator);
			iterator++;
			ImGui::PushID(uniqueId.c_str());
			ImGui::Selectable(label.c_str(), true);
			ImGui::PopID();
		}
	}

	iterator = 0;

	for (auto &device : state.devices)
	{
		if (device.trackingSystem != system)
			continue;

		auto label = LabelString(device);
		std::string uniqueId = label + "_pass1_" + std::to_string(iterator);
		iterator++;
		ImGui::PushID(uniqueId.c_str());
		if (ImGui::Selectable(label.c_str(), selected == device.id)) {
			selected = device.id;
		}
		ImGui::PopID();
	}
	if (selected != initialSelected) {
		const auto& device = std::find_if(state.devices.begin(), state.devices.end(), [&](const auto& d) { return d.id == selected; });
		if (device == state.devices.end()) return;

		initialSelected = selected;
		standbyDevice.trackingSystem = system;
		standbyDevice.model = device->model;
		standbyDevice.serial = device->serial;
	}
}

void BuildDeviceSelections(const VRState &state)
{
	ImGuiStyle &style = ImGui::GetStyle();
	ImVec2 paneSize(ImGui::GetWindowContentRegionWidth() / 2 - style.FramePadding.x, ImGui::GetTextLineHeightWithSpacing() * 5 + style.ItemSpacing.y * 4);

	ImGui::BeginChild("left device pane", paneSize, ImGuiChildFlags_Borders);
	BuildDeviceSelection(state, CalCtx.referenceID, CalCtx.referenceTrackingSystem, CalCtx.referenceStandby, true);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("right device pane", paneSize, ImGuiChildFlags_Borders);
	BuildDeviceSelection(state, CalCtx.targetID, CalCtx.targetTrackingSystem, CalCtx.targetStandby, false);
	ImGui::EndChild();

	if (ImGui::Button("Identify selected devices (blinks LED or vibrates)", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeightWithSpacing() + 4.0f)))
	{
		for (unsigned i = 0; i < 100; ++i)
		{
			vr::VRSystem()->TriggerHapticPulse(CalCtx.targetID, 0, 2000);
			vr::VRSystem()->TriggerHapticPulse(CalCtx.referenceID, 0, 2000);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
}

VRState LoadVRState() {
	VRState state = VRState::Load();
	auto& trackingSystems = state.trackingSystems;

	// Inject entries for continuous calibration targets which have yet to load

	if (CalCtx.state == CalibrationState::ContinuousStandby) {
		auto existing = std::find(trackingSystems.begin(), trackingSystems.end(), CalCtx.referenceTrackingSystem);
		if (existing == trackingSystems.end()) {
			trackingSystems.push_back(CalCtx.referenceTrackingSystem);
		}

		existing = std::find(trackingSystems.begin(), trackingSystems.end(), CalCtx.targetTrackingSystem);
		if (existing == trackingSystems.end()) {
			trackingSystems.push_back(CalCtx.targetTrackingSystem);
		}
	}

	return state;
}

void BuildProfileEditor()
{
	ImGuiStyle &style = ImGui::GetStyle();
	float width = ImGui::GetWindowContentRegionWidth() / 3.0f - style.FramePadding.x;
	float widthF = width - style.FramePadding.x;

	TextWithWidth("YawLabel", "Yaw", width);
	ImGui::SameLine();
	TextWithWidth("PitchLabel", "Pitch", width);
	ImGui::SameLine();
	TextWithWidth("RollLabel", "Roll", width);

	ImGui::PushItemWidth(widthF);
	ImGui::InputDouble("##Yaw", &CalCtx.calibratedRotation(1), 0.1, 1.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Pitch", &CalCtx.calibratedRotation(2), 0.1, 1.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Roll", &CalCtx.calibratedRotation(0), 0.1, 1.0, "%.8f");

	TextWithWidth("XLabel", "X", width);
	ImGui::SameLine();
	TextWithWidth("YLabel", "Y", width);
	ImGui::SameLine();
	TextWithWidth("ZLabel", "Z", width);

	ImGui::InputDouble("##X", &CalCtx.calibratedTranslation(0), 1.0, 10.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Y", &CalCtx.calibratedTranslation(1), 1.0, 10.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Z", &CalCtx.calibratedTranslation(2), 1.0, 10.0, "%.8f");

	TextWithWidth("ScaleLabel", "Scale", width);

	ImGui::InputDouble("##Scale", &CalCtx.calibratedScale, 0.0001, 0.01, "%.8f");
	ImGui::PopItemWidth();
}

void TextWithWidth(const char *label, const char *text, float width)
{
	ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
	ImGui::Text(text);
	ImGui::EndChild();
}

