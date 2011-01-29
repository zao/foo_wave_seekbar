#include "PchSeekbar.h"
#include "SeekbarWindow.h"

namespace wave
{
	bool has_direct3d9();
	bool has_direct2d1() { return true; }

	LRESULT seekbar_window::configuration_dialog::on_wm_init_dialog(ATL::CWindow focus, LPARAM lparam)
	{
		// Fill frontend combobox
		CComboBox cb = GetDlgItem(IDC_FRONTEND);
		std::wstring d3d = L"Direct3D 9.0c";
		std::wstring d2d = L"Direct2D 1.0";
		std::wstring gdi = L"GDI";
		
		auto add_frontend_string = [&cb](config::frontend frontend)
		{
			cb.SetItemData(cb.AddString(config::strings::frontend[frontend]), frontend);
		};

		if (has_direct3d9())
			add_frontend_string(config::frontend_direct3d9);
		if (has_direct2d1())
			add_frontend_string(config::frontend_direct2d1);
		add_frontend_string(config::frontend_gdi);

		auto select_frontend_string = [&cb](config::frontend frontend)
		{
			cb.SelectString(0, config::strings::frontend[frontend]);
		};

		select_frontend_string(sw.settings.active_frontend_kind);

		// Initialize colour pickers
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

		// Set up misc settings
		CheckDlgButton(IDC_NOBORDER, !sw.settings.has_border ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SHADEPLAYED, sw.settings.shade_played ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_MIRRORDISPLAY, sw.settings.flip_display ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_DOWNMIX, sw.settings.downmix_display ? BST_CHECKED : BST_UNCHECKED);

		// Set up display properties
		CComboBox modes = GetDlgItem(IDC_DISPLAYMODE);
		for (int i = 0; i < sizeof(config::strings::display_mode) / sizeof(wchar_t const*); ++i)
		{
			modes.SetItemData(modes.AddString(config::strings::display_mode[i]), (config::display_mode)i);
		}
		modes.SelectString(0, config::strings::display_mode[sw.settings.display_mode]);

		// Set up channel listings
		buttons.up = GetDlgItem(IDC_CHANNEL_UP);
		buttons.down = GetDlgItem(IDC_CHANNEL_DOWN);

		channels = GetDlgItem(IDC_CHANNELS);
		channels.SetExtendedListViewStyle(LVS_EX_CHECKBOXES);

		auto append = [this](std::wstring const& text, int data, bool checked)
		{
			LVITEM item = {};
			item.mask = LVIF_PARAM | LVIF_TEXT;
			item.pszText = const_cast<LPWSTR>(text.c_str());
			item.lParam = data;
			item.iItem = std::numeric_limits<int>::max();
			int idx = this->channels.InsertItem(&item);
			if (checked)
				this->channels.SetCheckState(idx, checked ? TRUE : FALSE);
		};

		for each(auto& pair in sw.settings.channel_order)
		{
			append(config::strings::channel_names[pair.first], pair.first, pair.second);
		}
		initializing = false;
		
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

		CButton config_button = GetDlgItem(IDC_CONFIGURE);
		bool has_conf = config::frontend_has_configuration[fe];
		config_button.EnableWindow(has_conf);

		if (initializing)
			return;

		// close frontend config window
		if (sw.fe->frontend)
			sw.fe->frontend->close_configuration();

		if (fe != sw.settings.active_frontend_kind)
		{
			sw.set_frontend(fe);
		}

	}

	void seekbar_window::configuration_dialog::on_no_border_click(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		sw.set_border_visibility(!IsDlgButtonChecked(id));
	}

	void seekbar_window::configuration_dialog::on_shade_played_click(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		sw.set_shade_played(!!IsDlgButtonChecked(id));
	}

	HBRUSH seekbar_window::configuration_dialog::on_wm_ctl_color_static(WTL::CDCHandle dc, ATL::CWindow wnd)
	{
		if (initializing)
			return 0;
		for (int i = 0; i < config::color_count; ++i)
			if (wnd == colors[i].box)
				return colors[i].box.IsWindowEnabled()
					? colors[i].brush
					: GetSysColorBrush(COLOR_BTNFACE);
		return 0;
	}

	void seekbar_window::configuration_dialog::on_color_click(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
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
		if (initializing)
			return;
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

	void seekbar_window::configuration_dialog::on_display_select(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		CComboBox cb = control;
		config::display_mode mode = (config::display_mode)cb.GetItemData(cb.GetCurSel());
		if (mode != sw.settings.display_mode)
		{
			sw.set_display_mode(mode);
		}
	}

	void seekbar_window::configuration_dialog::on_downmix_click(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		sw.set_downmix_display(!!IsDlgButtonChecked(id));
	}
	
	void seekbar_window::configuration_dialog::on_flip_click(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		sw.set_flip_display(!!IsDlgButtonChecked(id));
	}

	LRESULT seekbar_window::configuration_dialog::on_channel_changed(NMHDR* hdr)
	{
		if (initializing)
			return 0;
		NMLISTVIEW* nm = (NMLISTVIEW*)hdr;
		if (nm->uChanged & LVIF_STATE)
		{
			if (nm->uNewState & LVIS_SELECTED)
			{
				buttons.up.EnableWindow(nm->iItem > 0 ? TRUE : FALSE);
				buttons.down.EnableWindow(nm->iItem + 1 < channels.GetItemCount() ? TRUE : FALSE);
			}
			if (int state = (nm->uNewState >> 12 & 0xF)) // has checkbox state
			{
				int ch = channels.GetItemData(nm->iItem);
				//bool checked = !!channels.GetCheckState(nm->iItem);
				sw.set_channel_enabled(ch, !!(state >> 1));
			}
		}
		return 0;
	}

	LRESULT seekbar_window::configuration_dialog::on_channel_click(NMHDR* hdr)
	{
		if (initializing)
			return 0;
		NMITEMACTIVATE* nm = (NMITEMACTIVATE*)hdr;
		return 0;
	}

	void seekbar_window::configuration_dialog::swap_channels(int i1, int i2)
	{
		struct item
		{
			wchar_t buf[80];
			LVITEMW lvitem;
			explicit item(int idx)
			{
				UINT mask = LVIF_PARAM | LVIF_STATE | LVIF_TEXT;
				LVITEMW i = {mask, idx, 0, 0, (UINT)-1, buf, 80}; lvitem = i;
			}
			operator LVITEMW* () { return &lvitem; }
			LVITEM* operator -> () { return &lvitem; }
		} item1(i1), item2(i2);

		channels.GetItem(item1);
		channels.GetItem(item2);
		item1->iItem = i2;
		item2->iItem = i1;
		channels.SetItem(item1);
		channels.SetItem(item2);
	}

	void seekbar_window::configuration_dialog::on_channel_up(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		int idx = channels.GetSelectedIndex();
		if (idx == 0)
			return;
		int ch1 = channels.GetItemData(idx - 1);
		int ch2 = channels.GetItemData(idx);
		sw.swap_channel_order(ch1, ch2);
		swap_channels(idx - 1, idx);
		channels.SelectItem(idx - 1);
	}

	void seekbar_window::configuration_dialog::on_channel_down(UINT code, int id, CWindow control)
	{
		if (initializing)
			return;
		int idx = channels.GetSelectedIndex();
		int count = channels.GetItemCount();
		if (idx + 1 == count)
			return;
		int ch1 = channels.GetItemData(idx);
		int ch2 = channels.GetItemData(idx + 1);
		sw.swap_channel_order(ch1, ch2);
		swap_channels(idx, idx + 1);
		channels.SelectItem(idx + 1);
	}

	void seekbar_window::configuration_dialog::on_configure_click(UINT code, int id, CWindow control)
	{
		if (sw.fe && sw.fe->frontend)
		{
			sw.fe->frontend->show_configuration(sw);
		}
	}

	seekbar_window::configuration_dialog::configuration_dialog(seekbar_window& sw) 
		: sw(sw), initializing(true)
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

	void seekbar_window::configuration_dialog::add_item(channel_info const& info, CListBox& box)
	{
		box.SetItemData(box.AddString(info.text.c_str()), info.data);
	}

	void seekbar_window::configuration_dialog::remove_item(int idx, CListBox& box)
	{
		box.DeleteString(idx);
	}

	seekbar_window::configuration_dialog::channel_info seekbar_window::configuration_dialog::get_item(int idx, CListBox& box)
	{
		channel_info ret;
		ret.data = box.GetItemData(idx);
		CString s;
		box.GetText(idx, s);
		ret.text = s;
		return ret;
	}
}