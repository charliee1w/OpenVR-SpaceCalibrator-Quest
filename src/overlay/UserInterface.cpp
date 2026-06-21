#include "stdafx.h"
#include "UserInterface.h"
#include "Calibration.h"
#include "CalibrationChain.h"
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
	if (runningInOverlay)
	{
		ImGui::SameLine();
		ImGui::Text("- close VR overlay to use mouse");
	}
	ImGui::EndChild();
}

void CCal_BasicInfo();
void CCal_DrawSettings();

static void LabeledSliderFloat(const char* label, float* value, float minV, float maxV, const char* fmt, const char* tooltip = nullptr) {
	ImGui::Text("%s", label);
	ImGui::SameLine();
	ImGui::PushID(label);
	ImGui::SliderFloat("##slider", value, minV, maxV, fmt, 0);
	if (tooltip && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}
	ImGui::PopID();
}

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
		ScaledDragFloat("Yaw blend scale", CalCtx.alignmentSpeedParams.align_rot_speed_scale, 1.0, 0.1, 1.5, 0);

		ImGui::EndGroupPanel();
	}

	if (CalCtx.slamReference) {
		ImGui::BeginGroupPanel("Quest / SLAM tuning", panel_size);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("Tuning for wireless Quest / inside-out reference + lighthouse trackers.");
		ImGui::PopStyleColor();

		LabeledSliderFloat("Spike reject (m)", &CalCtx.continuousSpikeThresholdM, 0.01f, 0.15f, "%.3f",
			"Reject calibration samples when the target jumps farther than this in one frame.");
		float frozenFrames = (float)CalCtx.continuousFrozenFrameThreshold;
		LabeledSliderFloat("Frozen frame threshold", &frozenFrames, 1.0f, 12.0f, "%.0f",
			"Frames of frozen reference motion before treating offset as drift.");
		CalCtx.continuousFrozenFrameThreshold = (int)frozenFrames;

		LabeledSliderFloat("Guardian drift (mm)", &CalCtx.guardianDriftTransThresholdM, 0.005f, 0.08f, "%.3f",
			"Translation shift in stored vs live guardian before recalibration.");
		float guardianYawDeg = CalCtx.guardianDriftYawThresholdRad * 180.0f / static_cast<float>(EIGEN_PI);
		LabeledSliderFloat("Guardian drift (deg)", &guardianYawDeg, 1.0f, 15.0f, "%.1f",
			"Yaw shift in stored vs live guardian before recalibration.");
		CalCtx.guardianDriftYawThresholdRad = guardianYawDeg * static_cast<float>(EIGEN_PI / 180.0);

		float guardianConfirms = (float)CalCtx.guardianDriftConfirmChecks;
		LabeledSliderFloat("Guardian confirms", &guardianConfirms, 1.0f, 8.0f, "%.0f",
			"Consecutive guardian drift checks required before triggering recal.");
		CalCtx.guardianDriftConfirmChecks = (int)guardianConfirms;

		float guardianCooldown = (float)CalCtx.guardianDriftCooldownFrames;
		LabeledSliderFloat("Guardian cooldown (frames)", &guardianCooldown, 10.0f, 120.0f, "%.0f",
			"Frames to wait after a guardian-triggered recal before checking again.");
		CalCtx.guardianDriftCooldownFrames = (int)guardianCooldown;

		ImGui::Checkbox("Auto recal on guardian drift", &CalCtx.autoRecalOnGuardianDrift);
		ImGui::EndGroupPanel();

		ImGui::BeginGroupPanel("SLAM reference behavior", panel_size);
		ImGui::Checkbox("Trust target yaw", &CalCtx.trustTargetYaw);
		ImGui::SameLine();
		ImGui::Checkbox("Compensate pose latency", &CalCtx.compensatePoseTimeOffset);
		ImGui::Checkbox("Apply head model to reference", &CalCtx.applyHeadModelToReference);
		ImGui::SameLine();
		ImGui::Checkbox("Reject yaw drift poses", &CalCtx.rejectYawDriftPoses);
		ImGui::Checkbox("Pause on reference jitter", &CalCtx.pauseOnReferenceJitter);
		LabeledSliderFloat("Max ref pose latency (s)", &CalCtx.maxReferencePoseTimeOffset, 0.01f, 0.12f, "%.3f",
			"Drop reference samples whose poseTimeOffset exceeds this.");
		LabeledSliderFloat("Max pose time skew (s)", &CalCtx.maxPoseTimeSkew, 0.01f, 0.12f, "%.3f",
			"Maximum allowed timestamp skew between reference and target.");
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
			DrawVectorElement("cc_playspace_scale", "PLayspace Scale", &CalCtx.calibratedScale, 1, " 1 ");
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
		ImGui::TextDisabled("hyblocker");
		ImGui::TextDisabled("gore");

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

void CCal_BasicInfo() {
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
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF000080);
			status = "NOT FOUND";
		} else if (!CalCtx.ReferencePoseIsValidSimple()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFFFF0080);
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
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF000080);
			status = "NOT FOUND";
		}
		else if (!CalCtx.TargetPoseIsValidSimple()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFFFF0080);
			status = "NOT TRACKING";
		}
		else {
			status = "OK";
		}
		ImGui::Text("Status: %s", status);
		ImGui::EndGroup();

		ImGui::EndTable();
	}

	{
		const double errorMm = Metrics::error_currentCal.last();
		const double computeMs = Metrics::computationTime.last();
		const double refJit = Metrics::jitterRef.last();
		const double tgtJit = Metrics::jitterTarget.last();

		ImGui::BeginGroupPanel("Live metrics", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
		if (Metrics::error_currentCal.size() > 0) {
			ImGui::Text("Active error: %.2f mm", errorMm);
		} else {
			ImGui::TextDisabled("Active error: --");
		}
		if (Metrics::computationTime.size() > 0) {
			ImGui::Text("Frame compute: %.2f ms", computeMs);
		} else {
			ImGui::TextDisabled("Frame compute: --");
		}
		if (Metrics::jitterRef.size() > 0 || Metrics::jitterTarget.size() > 0) {
			ImGui::Text("Reference jitter: %.2f   Target jitter: %.2f", refJit, tgtJit);
		} else {
			ImGui::TextDisabled("Jitter: collect a few samples first");
		}
		ImGui::Text("Recal threshold: %.1f", CalCtx.continuousCalibrationThreshold);
		ImGui::EndGroupPanel();
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
	ImGui::Checkbox("Static recalibration", &CalCtx.enableStaticRecalibration);
	ImGui::SameLine();
	ImGui::Checkbox("Enable debug logs", &Metrics::enableLogs);
	ImGui::SameLine();
	ImGui::Checkbox("Lock relative transform", &CalCtx.lockRelativePosition);
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
		if (CalCtx.validProfile && !CalCtx.enabled)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1), "Reference (%s) HMD not detected, profile disabled", GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
			ImGui::Text("");
		}

		float width = ImGui::GetWindowContentRegionWidth(), scale = 1.0f;
		if (CalCtx.validProfile)
		{
			width -= style.FramePadding.x * 4.0f;
			scale = 1.0f / 4.0f;
		}

		if (ImGui::Button("Start Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
		{
			ImGui::OpenPopup("Calibration Progress");
			StartCalibration();
		}

		ImGui::SameLine();
		if (ImGui::Button("Continuous Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
			StartContinuousCalibration();
		}

		if (CalCtx.validProfile)
		{
			ImGui::SameLine();
			if (ImGui::Button("Edit Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				CalCtx.state = CalibrationState::Editing;
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear Calibration", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				CalCtx.Clear();
				SaveProfile(CalCtx);
			}
		}

		EnsureDefaultChain();
		if (ImGui::Checkbox("Autostart continuous calibration", &CalChains[0].autostartContinuous)) {
			SaveProfile(CalCtx);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Start continuous calibration automatically when SteamVR loads this profile.");
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

void BuildDeviceSelection(const VRState &state, int &initialSelected, const std::string &system, StandbyDevice &standbyDevice)
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
	BuildDeviceSelection(state, CalCtx.referenceID, CalCtx.referenceTrackingSystem, CalCtx.referenceStandby);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("right device pane", paneSize, ImGuiChildFlags_Borders);
	BuildDeviceSelection(state, CalCtx.targetID, CalCtx.targetTrackingSystem, CalCtx.targetStandby);
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

