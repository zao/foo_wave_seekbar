#include "PchSeekbar.h"
#include "SeekbarWindow.h"

namespace wave
{
	bool has_direct2d1() {
		HMODULE lib = LoadLibrary(L"d2d1");
		FreeLibrary(lib);
		return !!lib;
	}

	LRESULT seekbar_window::configuration_dialog::on_wm_init_dialog(ATL::CWindow focus, LPARAM lparam)
	{
		CComboBox cb = GetDlgItem(IDC_FRONTEND);
		std::wstring d3d = L"Direct3D 9.0c";
		std::wstring d2d = L"Direct2D 1.0";
		cb.SetItemData(cb.AddString(d3d.c_str()), config::frontend_direct3d9);
		if (has_direct2d1())
			cb.SetItemData(cb.AddString(d2d.c_str()), config::frontend_direct2d1);
		switch (sw.settings.active_frontend_kind)
		{
			case config::frontend_direct3d9:
				cb.SelectString(0, d3d.c_str());
				break;
			case config::frontend_direct2d1:
				cb.SelectString(0, d2d.c_str());
				break;
		}

		mk_color_info(config::color_background, IDC_COLOR_BACKGROUND, IDC_USE_BACKGROUND);
		mk_color_info(config::color_foreground, IDC_COLOR_FOREGROUND, IDC_USE_FOREGROUND);
		mk_color_info(config::color_highlight, IDC_COLOR_HIGHLIGHT, IDC_USE_HIGHLIGHT);
		mk_color_info(config::color_selection, IDC_COLOR_SELECTION, IDC_USE_SELECTION);

		for (size_t i = 0; i < config::color_count; ++i)
		{
			bool override = sw.settings.override_colors[i];
			CheckDlgButton(colors[i].use_id, override ? BST_CHECKED : BST_UNCHECKED);
			colors[i].box.EnableWindow(override);
			colors[i].box.InvalidateRect(0);
		}
		CheckDlgButton(IDC_NOBORDER, !sw.settings.has_border ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SHADEPLAYED, sw.settings.shade_played ? BST_CHECKED : BST_UNCHECKED);
		return TRUE;
	}

	void seekbar_window::configuration_dialog::on_wm_close()
	{
		DestroyWindow();
	}

	void seekbar_window::configuration_dialog::on_frontend_select(UINT code, int id, CWindow control)
	{
		CComboBox cb = control;
		config::frontend fe = (config::frontend)cb.GetItemData(cb.GetCurSel());
		if (fe != sw.settings.active_frontend_kind)
		{
			sw.set_frontend(fe);
		}
	}

	void seekbar_window::configuration_dialog::on_no_border_click(UINT code, int id, CWindow control)
	{
		sw.set_border_visibility(!IsDlgButtonChecked(id));
	}

	void seekbar_window::configuration_dialog::on_shade_played_click(UINT code, int id, CWindow control)
	{
		sw.set_shade_played(!!IsDlgButtonChecked(id));
	}

	HBRUSH seekbar_window::configuration_dialog::on_wm_ctl_color_static(WTL::CDCHandle dc, ATL::CWindow wnd)
	{
		for (int i = 0; i < config::color_count; ++i)
			if (wnd == colors[i].box)
				return colors[i].box.IsWindowEnabled()
					? colors[i].brush
					: GetSysColorBrush(COLOR_BTNFACE);
		return 0;
	}

	void seekbar_window::configuration_dialog::on_color_click(UINT code, int id, CWindow control)
	{
		config::color idx;
		switch (id)
		{
		case IDC_COLOR_BACKGROUND:
			idx = config::color_background;
			break;
		case IDC_COLOR_FOREGROUND:
			idx = config::color_foreground;
			break;
		case IDC_COLOR_HIGHLIGHT:
			idx = config::color_highlight;
			break;
		case IDC_COLOR_SELECTION:
			idx = config::color_selection;
			break;
		default:
			return;
		}
		color_info& ci = colors[idx];

		COLORREF c = color_to_xbgr(ci.color);
		COLORREF arr[16] = {};
		CHOOSECOLOR cc = {
			sizeof(CHOOSECOLOR),
			*this, 0,
			c, arr, CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT
		};

		if (ChooseColor(&cc))
		{
			ci.brush.DeleteObject();
			ci.brush.CreateSolidBrush(cc.rgbResult);
			ci.box.InvalidateRect(0);
			ci.color = xbgr_to_color(cc.rgbResult);
			sw.set_color(idx, ci.color, true);
		}
	}

	void seekbar_window::configuration_dialog::on_use_color_click(UINT code, int id, CWindow control)
	{
		config::color idx;
		switch (id)
		{
		case IDC_USE_BACKGROUND:
			idx = config::color_background;
			break;
		case IDC_USE_FOREGROUND:
			idx = config::color_foreground;
			break;
		case IDC_USE_HIGHLIGHT:
			idx = config::color_highlight;
			break;
		case IDC_USE_SELECTION:
			idx = config::color_selection;
			break;
		default:
			return;
		}
		bool override = !!IsDlgButtonChecked(id);
		sw.set_color_override(idx, override);
		colors[idx].box.EnableWindow(override);
		colors[idx].box.InvalidateRect(0);
	}

	seekbar_window::configuration_dialog::configuration_dialog(seekbar_window& sw) 
		: sw(sw)
	{}

	void seekbar_window::configuration_dialog::OnFinalMessage(HWND wnd)
	{
		sw.config_dialog.reset();
	}

	void seekbar_window::configuration_dialog::mk_color_info(config::color what, UINT display_id, UINT use_id)
	{
		color c = sw.settings.colors[what];
		color_info& ci = colors[what];

		ci.box = GetDlgItem(display_id);
		ci.brush.CreateSolidBrush(color_to_xbgr(c));
		ci.color = c;
		ci.display_id = display_id;
		ci.use_id = use_id;
	}
}