#include "ui_theme.h"

namespace SpaceCalUI {
	namespace {
		ImVec4 g_accent = ImVec4(0.24f, 0.86f, 0.75f, 1.0f);
		ImVec4 g_accentMuted = ImVec4(0.14f, 0.45f, 0.42f, 1.0f);
		ImVec4 g_statusOk = ImVec4(0.30f, 0.85f, 0.55f, 1.0f);
		ImVec4 g_statusWarn = ImVec4(0.98f, 0.75f, 0.25f, 1.0f);
		ImVec4 g_statusError = ImVec4(0.97f, 0.45f, 0.45f, 1.0f);
		ImVec4 g_statusIdle = ImVec4(0.55f, 0.60f, 0.68f, 1.0f);
	}

	void ApplyTheme() {
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		colors[ImGuiCol_Text] = ImVec4(0.90f, 0.93f, 0.96f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.57f, 0.64f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.07f, 0.09f, 0.98f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.10f, 0.12f, 0.98f);
		colors[ImGuiCol_Border] = ImVec4(0.20f, 0.26f, 0.32f, 0.65f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.14f, 0.17f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.19f, 0.23f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.23f, 0.28f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.07f, 0.09f, 0.75f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.07f, 0.09f, 0.60f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = g_accentMuted;
		colors[ImGuiCol_ScrollbarGrabActive] = g_accent;
		colors[ImGuiCol_CheckMark] = g_accent;
		colors[ImGuiCol_SliderGrab] = g_accentMuted;
		colors[ImGuiCol_SliderGrabActive] = g_accent;
		colors[ImGuiCol_Button] = ImVec4(0.14f, 0.18f, 0.22f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.24f, 0.29f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.12f, 0.16f, 0.20f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.21f, 0.26f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.14f, 0.40f, 0.37f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.26f, 0.32f, 0.55f);
		colors[ImGuiCol_SeparatorHovered] = g_accentMuted;
		colors[ImGuiCol_SeparatorActive] = g_accent;
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.26f, 0.32f, 0.35f);
		colors[ImGuiCol_ResizeGripHovered] = g_accentMuted;
		colors[ImGuiCol_ResizeGripActive] = g_accent;
		colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.13f, 0.16f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.21f, 0.26f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.34f, 0.31f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.28f, 0.26f, 1.00f);
		colors[ImGuiCol_PlotLines] = g_accent;
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.35f, 0.95f, 0.85f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = g_accentMuted;
		colors[ImGuiCol_PlotHistogramHovered] = g_accent;
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.13f, 0.16f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.26f, 0.32f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.20f, 0.25f, 1.00f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.14f, 0.40f, 0.37f, 0.55f);
		colors[ImGuiCol_DragDropTarget] = g_accent;
		colors[ImGuiCol_NavHighlight] = g_accent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.90f, 0.93f, 0.96f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);

		style.WindowRounding = 6.0f;
		style.ChildRounding = 5.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 5.0f;
		style.ScrollbarRounding = 6.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		style.WindowPadding = ImVec2(14.0f, 12.0f);
		style.FramePadding = ImVec2(10.0f, 6.0f);
		style.ItemSpacing = ImVec2(10.0f, 8.0f);
		style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
		style.CellPadding = ImVec2(8.0f, 6.0f);
		style.IndentSpacing = 22.0f;
		style.ScrollbarSize = 14.0f;
		style.GrabMinSize = 12.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
	}

	ImVec4 Accent() { return g_accent; }
	ImVec4 AccentMuted() { return g_accentMuted; }
	ImVec4 StatusOk() { return g_statusOk; }
	ImVec4 StatusWarn() { return g_statusWarn; }
	ImVec4 StatusError() { return g_statusError; }
	ImVec4 StatusIdle() { return g_statusIdle; }

	ImU32 ColorToU32(const ImVec4& color, float alphaMul) {
		ImVec4 c = color;
		c.w *= alphaMul;
		return ImGui::ColorConvertFloat4ToU32(c);
	}

	bool PrimaryButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.32f, 0.29f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.42f, 0.38f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.26f, 0.24f, 1.0f));
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}

	bool DangerButton(const char* label, const ImVec2& size) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.16f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.10f, 0.10f, 1.0f));
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}
}