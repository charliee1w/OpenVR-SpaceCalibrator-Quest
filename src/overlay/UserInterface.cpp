#include "stdafx.h"
#include "UserInterface.h"
#include "CalibrationChain.h"
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
#include <cstdio>

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
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::Separator();
	ImGui::Text("v" SPACECAL_VERSION_STRING);
	ImGui::PopStyleColor();
	if (runningInOverlay)
	{
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextUnformatted("  |  close overlay for mouse input");
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();
}

void CCal_BasicInfo();
void CCal_DrawSettings();

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

	ImGui::SectionHeading("Continuous calibration",
		"Keeping your Quest and lighthouse playspaces aligned in real time.");

	if (ImGui::BeginTabBar("CCalTabs", ImGuiTabBarFlags_None)) {
		if (ImGui::BeginTabItem("Status")) {
			CCal_BasicInfo();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Graphs")) {
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

static void DrawDeviceStatusCard(const char* id, const char* role, const char* trackingSystem,
	const char* model, const char* serial, bool found, bool tracking)
{
	ImGui::PushID(id);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	float width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	float height = ImGui::GetFrameHeight() * 5.0f;
	ImVec2 size(width, height);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec4 accent = SpaceCalUI::StatusError();
	const char* status = "NOT FOUND";
	if (found) {
		if (tracking) {
			accent = SpaceCalUI::StatusOk();
			status = "TRACKING";
		} else {
			accent = SpaceCalUI::StatusWarn();
			status = "NOT TRACKING";
		}
	}

	drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
		SpaceCalUI::ColorToU32(ImVec4(0.10f, 0.13f, 0.16f, 1.0f)), 6.0f);
	drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
		SpaceCalUI::ColorToU32(ImVec4(0.20f, 0.26f, 0.32f, 0.65f)), 6.0f);
	drawList->AddRectFilled(pos, ImVec2(pos.x + 4.0f, pos.y + size.y),
		SpaceCalUI::ColorToU32(accent, 0.95f), 3.0f);

	ImGui::Dummy(size);
	ImGui::SetCursorScreenPos(ImVec2(pos.x + 14.0f, pos.y + 10.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, SpaceCalUI::Accent());
	ImGui::TextUnformatted(role);
	ImGui::PopStyleColor();
	ImGui::SetCursorScreenPos(ImVec2(pos.x + 14.0f, pos.y + 10.0f + ImGui::GetTextLineHeight() + 4.0f));
	ImGui::TextWrapped("%s", trackingSystem);
	ImGui::SetCursorScreenPos(ImVec2(pos.x + 14.0f, pos.y + 10.0f + (ImGui::GetTextLineHeight() + 4.0f) * 2.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("%s / %s", model, serial);
	ImGui::PopStyleColor();
	ImGui::SetCursorScreenPos(ImVec2(pos.x + 14.0f, pos.y + size.y - ImGui::GetTextLineHeight() - 10.0f));
	ImGui::StatusBadge(status, SpaceCalUI::ColorToU32(accent, 0.85f));
	ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x + ImGui::GetStyle().ItemSpacing.x, pos.y));
	ImGui::PopID();
}

static void DrawLiveMetricsRow() {
	char buf[64];
	const double errorMm = Metrics::error_currentCal.last();
	const double refJitter = Metrics::jitterRef.last();
	const double targetJitter = Metrics::jitterTarget.last();
	const double computeMs = Metrics::computationTime.last();

	const float cardH = ImGui::GetFrameHeight() * 3.2f;
	const float rowGap = ImGui::GetStyle().ItemSpacing.y;
	const ImVec2 rowStart = ImGui::GetCursorScreenPos();

	snprintf(buf, sizeof(buf), "%.1f", errorMm);
	ImGui::MetricCard("metric_error", "Active error", buf, "mm",
		errorMm < 10.0 ? SpaceCalUI::StatusOk() : (errorMm < 25.0 ? SpaceCalUI::StatusWarn() : SpaceCalUI::StatusError()));
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	snprintf(buf, sizeof(buf), "%.2f", computeMs);
	ImGui::MetricCard("metric_compute", "Frame compute", buf, "ms", SpaceCalUI::AccentMuted());

	ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + cardH + rowGap));
	snprintf(buf, sizeof(buf), "%.2f", refJitter);
	ImGui::MetricCard("metric_ref_jitter", "Reference jitter", buf, "",
		refJitter < CalCtx.jitterThreshold ? SpaceCalUI::StatusOk() : SpaceCalUI::StatusWarn());
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	snprintf(buf, sizeof(buf), "%.2f", targetJitter);
	ImGui::MetricCard("metric_tgt_jitter", "Target jitter", buf, "",
		targetJitter < CalCtx.jitterThreshold ? SpaceCalUI::StatusOk() : SpaceCalUI::StatusWarn());

	ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + (cardH + rowGap) * 2.0f));
	snprintf(buf, sizeof(buf), "%.0f", CalCtx.continuousCalibrationThreshold);
	ImGui::MetricCard("metric_recal", "Recal threshold", buf, "", SpaceCalUI::Accent());
	ImGui::Dummy(ImVec2(0.0f, cardH));
}

void CCal_DrawSettings() {

	ImVec2 panel_size { ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x, 0 };

	ImGui::HintText("Quest / SLAM tuning is open by default. Advanced alignment controls are collapsed unless you need them.");

	if (ImGui::CollapsingHeader("Quest / SLAM tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginGroupPanel("Stability filters", panel_size);
		ImGui::LabeledSliderFloat("Jitter threshold", &CalCtx.jitterThreshold, 0.1f, 10.0f, "%.2f",
			"How much tracking jitter is tolerated before samples are rejected. Lower = stricter.");
		ImGui::LabeledSliderFloat("Spike reject (m)", &CalCtx.continuousSpikeThresholdM, 0.01f, 0.15f, "%.3f",
			"Max head-offset jump between samples. Adaptive scaling still applies on SLAM references.");
		float alignRotSpeedScale = (float)CalCtx.alignmentSpeedParams.align_rot_speed_scale;
		if (ImGui::LabeledSliderFloat("Yaw blend scale", &alignRotSpeedScale, 0.1f, 1.5f, "%.2f",
			"Driver rotation lerp vs translation. Lower = slower yaw corrections (~0.45 for Quest).")) {
			CalCtx.alignmentSpeedParams.align_rot_speed_scale = alignRotSpeedScale;
		}
		ImGui::LabeledSliderFloat("Guardian drift (mm)", &CalCtx.guardianDriftTransThresholdM, 0.005f, 0.08f, "%.3f",
			"Translation delta that triggers a guardian geometry restore.");
		float guardianYawDeg = CalCtx.guardianDriftYawThresholdRad * 180.0f / static_cast<float>(EIGEN_PI);
		if (ImGui::LabeledSliderFloat("Guardian drift (deg)", &guardianYawDeg, 1.0f, 15.0f, "%.1f",
			"Yaw delta that triggers a guardian geometry restore.")) {
			CalCtx.guardianDriftYawThresholdRad = guardianYawDeg * static_cast<float>(EIGEN_PI / 180.0);
		}
		float guardianConfirms = (float)CalCtx.guardianDriftConfirmChecks;
		if (ImGui::LabeledSliderFloat("Guardian confirms", &guardianConfirms, 1.0f, 8.0f, "%.0f",
			"Consecutive checks required before applying a guardian restore.")) {
			CalCtx.guardianDriftConfirmChecks = (int)(guardianConfirms + 0.5f);
		}
		ImGui::EndGroupPanel();
	}

	if (ImGui::CollapsingHeader("Recalibration", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginGroupPanel("When to realign", panel_size);
		ImGui::LabeledSliderFloat("Recalibration threshold", &CalCtx.continuousCalibrationThreshold, 1.01f, 10.0f, "%.1f",
			"Higher = less frequent realignment. Useful when drift is slow and stable.");
		ImGui::LabeledSliderFloat("Max relative error", &CalCtx.maxRelativeErrorThreshold, 0.01f, 1.0f, "%.2f",
			"Poor relative-calibration results above this are discarded.");
		ImGui::HintText("Controls how often SpaceCalibrator synchronises playspaces.");
		ImGui::EndGroupPanel();
	}

	if (ImGui::CollapsingHeader("Offsets & scale", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImVec2 panel_size_inner { panel_size.x - 11 * 2, 0 };
		ImGui::BeginGroupPanel("Tracker offset", panel_size_inner);
		DrawVectorElement("cc_tracker_offset", "X", &CalCtx.continuousCalibrationOffset.x());
		DrawVectorElement("cc_tracker_offset", "Y", &CalCtx.continuousCalibrationOffset.y());
		DrawVectorElement("cc_tracker_offset", "Z", &CalCtx.continuousCalibrationOffset.z());
		ImGui::EndGroupPanel();

		ImGui::BeginGroupPanel("Playspace scale", panel_size_inner);
		DrawVectorElement("cc_playspace_scale", "Scale", &CalCtx.calibratedScale, 1, " 1 ");
		ImGui::EndGroupPanel();
	}

	if (ImGui::CollapsingHeader("Advanced alignment", ImGuiTreeNodeFlags_None)) {
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

		ImGui::BeginGroupPanel("Alignment speeds", panel_size);
		ScaledDragFloat("Decel", CalCtx.alignmentSpeedParams.align_speed_tiny, 1.0, 0, 2.0, 0);
		ScaledDragFloat("Slow", CalCtx.alignmentSpeedParams.align_speed_small, 1.0, 0, 2.0, 0);
		ScaledDragFloat("Fast", CalCtx.alignmentSpeedParams.align_speed_large, 1.0, 0, 2.0, 0);
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

void CCal_BasicInfo() {
	const char* refTrackingSystem = GetPrettyTrackingSystemName(CalCtx.referenceStandby.trackingSystem);
	const char* targetTrackingSystem = GetPrettyTrackingSystemName(CalCtx.targetStandby.trackingSystem);
	const bool refFound = CalCtx.referenceID >= 0;
	const bool tgtFound = CalCtx.targetID >= 0;
	const bool refTracking = refFound && CalCtx.ReferencePoseIsValidSimple();
	const bool tgtTracking = tgtFound && CalCtx.TargetPoseIsValidSimple();

	const float deviceCardH = ImGui::GetFrameHeight() * 5.0f;
	DrawDeviceStatusCard("ref_card", "Reference", refTrackingSystem,
		CalCtx.referenceStandby.model.c_str(), CalCtx.referenceStandby.serial.c_str(),
		refFound, refTracking);
	DrawDeviceStatusCard("tgt_card", "Target", targetTrackingSystem,
		CalCtx.targetStandby.model.c_str(), CalCtx.targetStandby.serial.c_str(),
		tgtFound, tgtTracking);
	ImGui::Dummy(ImVec2(0.0f, deviceCardH + ImGui::GetStyle().ItemSpacing.y * 2.0f));

	ImGui::BeginGroupPanel("Live metrics", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
	DrawLiveMetricsRow();
	ImGui::EndGroupPanel();

	float width = ImGui::GetWindowContentRegionWidth(), scale = 1.0f;
	float btnHeight = ImGui::GetTextLineHeight() * 2.2f;

	if (ImGui::BeginTable("##CCal_Cancel", Metrics::enableLogs ? 3 : 2, 0, ImVec2(width * scale, btnHeight))) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (SpaceCalUI::DangerButton("Stop continuous cal", ImVec2(-FLT_MIN, 0.0f))) {
			EndContinuousCalibration();
		}

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("Debug: break cal", ImVec2(-FLT_MIN, 0.0f))) {
			DebugApplyRandomOffset();
		}

		if (Metrics::enableLogs) {
			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button("Mark logs", ImVec2(-FLT_MIN, 0.0f))) {
				Metrics::WriteLogAnnotation("MARK LOGS");
			}
		}

		ImGui::EndTable();
	}

	if (CalChains.size() > 1) {
		ImGui::BeginGroupPanel("Multi-platform chains", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
		ImGui::Text("Active chains: %zu", CalChains.size());
		for (size_t i = 0; i < CalChains.size(); i++) {
			const auto& chain = CalChains[i];
			ImGui::TextDisabled("  [%zu] %s -> %s%s",
				i,
				chain.referenceTrackingSystem.c_str(),
				chain.targetTrackingSystem.c_str(),
				chain.continuousActive ? " (cont)" : "");
		}
		ImGui::EndGroupPanel();
	}

	if (ImGui::CollapsingHeader("Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Lock relative transform", &CalCtx.lockRelativePosition);
		ImGui::SameLine();
		ImGui::Checkbox("Hide tracker", &CalCtx.quashTargetInContinuous);
		ImGui::Checkbox("Static recalibration", &CalCtx.enableStaticRecalibration);
		ImGui::SameLine();
		ImGui::Checkbox("Ignore outliers", &CalCtx.ignoreOutliers);
		ImGui::Checkbox("Require triggers", &CalCtx.requireTriggerPressToApply);
		ImGui::SameLine();
		ImGui::Checkbox("Enable debug logs", &Metrics::enableLogs);
	}

	ImGui::BeginGroupPanel("Activity log", ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.07f, 1.0f));
	if (ImGui::BeginChild("ccal_log", ImVec2(0, ImGui::GetTextLineHeight() * 4.5f), ImGuiChildFlags_Borders)) {
		for (const auto& msg : CalCtx.messages) {
			if (msg.type == CalibrationContext::Message::String) {
				ImGui::PushStyleColor(ImGuiCol_Text, SpaceCalUI::AccentMuted());
				ImGui::TextUnformatted(">");
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::TextWrapped("%s", msg.str.c_str());
			}
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::EndGroupPanel();

	ShowCalibrationDebug(1, 3);
}

void BuildMenu(bool runningInOverlay)
{
	auto &io = ImGui::GetIO();
	ImGuiStyle &style = ImGui::GetStyle();
	ImGui::Text("");

	if (CalCtx.state == CalibrationState::None)
	{
		ImGui::SectionHeading("Space Calibrator",
			"Align Quest / SLAM head tracking with SteamVR lighthouse body tracking.");

		if (CalCtx.validProfile && !CalCtx.enabled)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, SpaceCalUI::StatusError());
			ImGui::TextWrapped("Reference (%s) HMD not detected — saved profile is disabled.",
				GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
			ImGui::PopStyleColor();
			ImGui::Spacing();
		}

		float width = ImGui::GetWindowContentRegionWidth(), scale = 1.0f;
		float btnHeight = ImGui::GetTextLineHeight() * 2.2f;
		if (CalCtx.validProfile)
		{
			width -= style.FramePadding.x * 4.0f;
			scale = 1.0f / 4.0f;
		}

		if (ImGui::Button("One-shot calibration", ImVec2(width * scale, btnHeight)))
		{
			ImGui::OpenPopup("Calibration Progress");
			StartCalibration();
		}

		ImGui::SameLine();
		if (SpaceCalUI::PrimaryButton("Continuous calibration", ImVec2(width * scale, btnHeight))) {
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

