#pragma once

#include <memory>

struct ImFont;

// Owns the ImGui theme, font atlas and the full menu render tree.
// One global instance (g_menu). The menu is drawn on the render thread.
class c_menu {
	int m_selected_tab = 0;
	bool m_style_initialized = false;
	float m_dpi_scale = 1.0f;
	ImFont* m_font = nullptr;

public:
	bool m_opened = true;

	// Render the full window for the current frame. No-op when closed.
	void draw();
	void release_resources();

	// Rebuild the font atlas for a new DPI scale and re-apply the theme.
	void rebuild_fonts(float scale);

	bool is_open() const { return m_opened; }

private:
	void setup_style();
};

inline const auto g_menu = std::make_unique<c_menu>();
