#pragma once

#include <imgui.h>

namespace SpaceCalUI {
	void ApplyTheme();

	ImVec4 Accent();
	ImVec4 AccentMuted();
	ImVec4 StatusOk();
	ImVec4 StatusWarn();
	ImVec4 StatusError();
	ImVec4 StatusIdle();

	ImU32 ColorToU32(const ImVec4& color, float alphaMul = 1.0f);

	bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));
}