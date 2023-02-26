//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D11.h"
#include "Direct3D11.h"
#include "Direct3D11.Effects.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include <scintilla/Scintilla.h>
#include <sstream>

namespace wave {
namespace direct3d11 {
void
config_dialog::scintilla_func::init(HWND wnd)
{
    sptr_t (*fn)(void*, unsigned int, uptr_t, sptr_t);
    void* ptr;

    fn = (decltype(fn))SendMessage(wnd, SCI_GETDIRECTFUNCTION, 0, 0);
    ptr = (decltype(ptr))SendMessage(wnd, SCI_GETDIRECTPOINTER, 0, 0);

    using namespace std::placeholders;
    f = std::bind(fn, ptr, _1, _2, _3);
}

config_dialog::config_dialog(ref_ptr<frontend_impl> fe)
  : fe(fe)
{
    auto front = fe;
    front->get_effect_compiler(compiler);
}

LRESULT
config_dialog::on_wm_init_dialog(ATL::CWindow focus, LPARAM param)
{
    auto front = fe;
    if (!front)
        return FALSE;

    // set up monospaced fonts
    code_box.init(GetDlgItem(IDC_EFFECT_SOURCE));
    error_box = GetDlgItem(IDC_EFFECT_ERRORS);

    apply_button = GetDlgItem(IDC_EFFECT_APPLY);
    default_button = GetDlgItem(IDC_EFFECT_DEFAULT);
    reset_button = GetDlgItem(IDC_EFFECT_RESET);

    mono_font.CreateFontW(0,
                          0,
                          0,
                          0,
                          FW_DONTCARE,
                          FALSE,
                          FALSE,
                          FALSE,
                          DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY,
                          FIXED_PITCH | FF_DONTCARE,
                          nullptr);
    error_box.SetFont(mono_font);
    {
        code_box.send(SCI_STYLESETFONT, STYLE_DEFAULT, "Courier New");
        code_box.send(SCI_STYLECLEARALL);
        for (int i = 0; i <= 4; ++i)
            code_box.send(SCI_SETMARGINWIDTHN, i, 0);
        code_box.send(SCI_SETMARGINWIDTHN, 0, 24);
        code_box.send(SCI_SETMARGINWIDTHN, 1, 8);
    }

    on_effect_reset_click(BN_CLICKED, IDC_EFFECT_RESET, reset_button);

    // compile effect
    PostMessage(WM_COMMAND, MAKEWPARAM(9001, EN_CHANGE), (LPARAM)(HWND)GetDlgItem(IDC_EFFECT_SOURCE));

    return TRUE;
}

void
config_dialog::on_wm_close()
{
    DestroyWindow();
}

void
config_dialog::on_effect_apply_click(UINT code, int id, CWindow control)
{
    if (auto front = fe) {
        front->set_effect(fx, true);

        std::string source;
        code_box.get_all(source);
        front->conf.set_configuration_string(guid_fx_string, source.c_str());
        apply_button.EnableWindow(FALSE);
        reset_button.EnableWindow(FALSE);
    }
}

void
config_dialog::on_effect_default_click(UINT code, int id, CWindow control)
{
    std::vector<char> fx_body;
    get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
    code_box.reset(fx_body);
    on_effect_source_change(EN_CHANGE, IDC_EFFECT_SOURCE);
    apply_button.EnableWindow(TRUE);
    reset_button.EnableWindow(TRUE);
}

void
config_dialog::on_effect_reset_click(UINT code, int id, CWindow control)
{
    if (auto front = fe) {
        // read up effect contents
        std::string fx_data;
        front->conf.get_configuration_string(guid_fx_string, std_string_sink(fx_data));

        // set effect box text
        code_box.reset(fx_data);
    }
    apply_button.EnableWindow(FALSE);
    reset_button.EnableWindow(FALSE);
}

LRESULT
config_dialog::on_effect_source_modified(NMHDR* hdr)
{
    SCNotification* n = (SCNotification*)hdr;

    if (n->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
        PostMessage(WM_COMMAND, MAKEWPARAM(9001, EN_CHANGE), (LPARAM)(HWND)GetDlgItem(IDC_EFFECT_SOURCE));

    return 0;
}

void
config_dialog::on_effect_source_change(UINT code, int id, CEdit)
{
    CEdit error_box{ GetDlgItem(IDC_EFFECT_ERRORS) };
    std::string source;
    code_box.get_all(source);

    if (source.empty())
        return;

    code_box.clear_annotations();
    std::deque<diagnostic_collector::entry> output;
    bool success = compiler->compile_fragment(fx, diagnostic_collector(output), source.c_str(), source.size());
    if (success) {
        error_box.SetWindowTextW(L"No errors.\n");
    } else {
        std::ostringstream errors;
        std::for_each(
          begin(output), end(output), [&](diagnostic_collector::entry e) { errors << e.line << std::endl; });
        error_box.SetWindowTextW(pfc::stringcvt::string_wide_from_utf8(errors.str().c_str()));
    }
    error_box.SetSelNone(FALSE);

    if (auto front = fe) {
        front->set_effect(fx, false);
        bool ok = fx;
        apply_button.EnableWindow(ok);
        reset_button.EnableWindow(TRUE);
    }
}

void
config_dialog::OnFinalMessage(HWND wnd)
{
    // reset parents pointer to dialog
    auto front = fe;
    if (front)
        front->config.reset();
}
}
}
