#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
#include "../frontend_sdk/FrontendHelpers.h"

namespace wave
{
	namespace direct3d9
	{
		config_dialog::config_dialog(weak_ptr<frontend_impl> fe)
			: fe(fe)
		{
			auto front = fe.lock();
			front->get_effect_compiler(compiler);
		}

		LRESULT config_dialog::on_wm_init_dialog(ATL::CWindow focus, LPARAM param)
		{
			auto front = fe.lock();
			if (!front)
				return FALSE;

			// set up monospaced fonts
			code_box = GetDlgItem(IDC_EFFECT_SOURCE);
			error_box = GetDlgItem(IDC_EFFECT_ERRORS);

			apply_button = GetDlgItem(IDC_EFFECT_APPLY);
			default_button = GetDlgItem(IDC_EFFECT_DEFAULT);
			reset_button = GetDlgItem(IDC_EFFECT_RESET);

			mono_font.CreateFontW(0, 0, 0, 0,
				FW_DONTCARE, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				FIXED_PITCH | FF_DONTCARE,
				nullptr);
			code_box.SetFont(mono_font);
			error_box.SetFont(mono_font);

			on_effect_reset_click(BN_CLICKED, IDC_EFFECT_RESET, reset_button);

			// compile effect
			PostMessage(WM_COMMAND, MAKEWPARAM(IDC_EFFECT_SOURCE, EN_CHANGE), (LPARAM)(HWND)GetDlgItem(IDC_EFFECT_SOURCE));

			return TRUE;
		}

		void config_dialog::on_wm_close()
		{
			DestroyWindow();
		}

		void config_dialog::on_effect_apply_click(UINT code, int id, CWindow control)
		{
			if (auto front = fe.lock())
			{
				front->set_effect(fx, true);

				ATL::CString source;
				code_box.GetWindowTextW(source);
				front->conf.set_configuration_string(guid_fx_string, pfc::stringcvt::string_utf8_from_wide(source, source.GetLength()).get_ptr());
				apply_button.EnableWindow(FALSE);
				reset_button.EnableWindow(FALSE);
			}
		}

		void config_dialog::on_effect_default_click(UINT code, int id, CWindow control)
		{
			std::vector<char> fx_body;
			get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
			code_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(&fx_body[0], fx_body.size()));
			on_effect_source_change(EN_CHANGE, IDC_EFFECT_SOURCE, code_box);
			apply_button.EnableWindow(TRUE);
			reset_button.EnableWindow(TRUE);
		}

		void config_dialog::on_effect_reset_click(UINT code, int id, CWindow control)
		{
			if (auto front = fe.lock())
			{
				// read up effect contents
				std::string fx_data;
				front->conf.get_configuration_string(guid_fx_string, fx_data);
		
				// set effect box text
				code_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(fx_data.c_str()).get_ptr());
				PostMessage(WM_USER_CLEAR_EFFECT_SELECTION);
			}
			apply_button.EnableWindow(FALSE);
			reset_button.EnableWindow(FALSE);
		}

		LRESULT config_dialog::on_clear_effect_selection(UINT, WPARAM, LPARAM, BOOL& handled)
		{
			CEdit code_box = GetDlgItem(IDC_EFFECT_SOURCE);
			code_box.SetSelNone(TRUE);
			handled = TRUE;
			return TRUE;
		}

		void config_dialog::on_effect_source_change(UINT code, int id, CEdit control)
		{
			CEdit error_box = GetDlgItem(IDC_EFFECT_ERRORS);
			ATL::CString source;
			control.GetWindowTextW(source);

			std::deque<effect_compiler::diagnostic_entry> output;
			bool success = compiler->compile_fragment(fx, output, pfc::stringcvt::string_utf8_from_wide(source).get_ptr());
			if (success)
			{
				error_box.SetWindowTextW(L"No errors.\n");
			}
			else
			{
				std::string errors = simple_diagnostic_format(output);
				error_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(errors.c_str()));
			}
			error_box.SetSelNone(FALSE);

			if (auto front = fe.lock())
			{
				front->set_effect(fx, false);
				bool ok = fx;
				apply_button.EnableWindow(ok);
				reset_button.EnableWindow(TRUE);
			}
		}

		void config_dialog::OnFinalMessage(HWND wnd)
		{
			// reset parents pointer to dialog
			auto front = fe.lock();
			if (front)
				front->config.reset();
		}
	}
}