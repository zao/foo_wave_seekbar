#include "PchSeekbar.h"
#include "Direct3D.h"

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
			CEdit code_box = GetDlgItem(IDC_EFFECT_SOURCE);
			CEdit error_box = GetDlgItem(IDC_EFFECT_ERRORS);

			mono_font.CreateFontW(0, 0, 0, 0,
				FW_DONTCARE, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				FIXED_PITCH | FF_DONTCARE,
				nullptr);
			code_box.SetFont(mono_font);
			error_box.SetFont(mono_font);

			// read up effect contents
			pfc::string fx_data;
			front->conf.get_configuration_string(guid_fx_string, fx_data);
		
			// set effect box text
			code_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(fx_data.get_ptr()).get_ptr());
			PostMessage(WM_USER_CLEAR_EFFECT_SELECTION);

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
		}

		LRESULT config_dialog::on_clear_effect_selection(UINT, WPARAM, LPARAM, BOOL& handled)
		{
			CEdit code_box = GetDlgItem(IDC_EFFECT_SOURCE);
			code_box.SetSelNone(TRUE);
			handled = TRUE;
			return TRUE;
		}

		pfc::string flatten(pfc::list_t<effect_compiler::diagnostic_entry> const& in)
		{
			using karma::int_;
			using karma::string;

			typedef std::back_insert_iterator<std::string> Iter;
			karma::rule<Iter, effect_compiler::diagnostic_entry::location()> loc = '(' << int_ << ',' << int_ << "): ";

			std::vector<std::string> lines;
			in.enumerate([&lines, &loc](effect_compiler::diagnostic_entry const& e)
			{
				std::string out;
				auto sink = std::back_inserter(out);

				karma::generate(sink,
					loc << string << ": " << string << ": " << string , e);
				lines.push_back(out);
			});

			std::string out;
			auto sink = std::back_inserter(out);
			karma::generate(sink, string % "\n", lines);
			return pfc::string(out.c_str());
		}

		void config_dialog::on_effect_source_change(UINT code, int id, CEdit control)
		{
			CEdit error_box = GetDlgItem(IDC_EFFECT_ERRORS);
			ATL::CString source;
			control.GetWindowTextW(source);

			service_ptr_t<effect_handle> fx;
			pfc::list_t<effect_compiler::diagnostic_entry> output;
			bool success = compiler->compile_fragment(fx, output, pfc::stringcvt::string_utf8_from_wide(source));
			if (success)
				error_box.SetWindowTextW(L"No errors.\n");
			else
			{
				pfc::string errors = flatten(output);
				error_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(errors.get_ptr()));
			}
			error_box.SetSelNone(FALSE);
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