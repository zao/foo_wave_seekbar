//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../frontend_sdk/VisualFrontend.h"
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include "resource.h"
#include "Scintilla.h"

namespace wave {
bool
has_direct3d9();

struct duration_query
{
  LARGE_INTEGER then;
  duration_query() { QueryPerformanceCounter(&then); }

  double get_elapsed() const
  {
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    return (double)(now.QuadPart - then.QuadPart) / (double)freq.QuadPart;
  }
};

namespace direct3d9 {
struct config_dialog;
struct effect_compiler;
struct effect_handle;

extern const GUID guid_fx_string;

struct d3d9_api
{
  HMODULE d3d9_module = nullptr;
  decltype(Direct3DCreate9)* Direct3DCreate9 = nullptr;

  HMODULE d3dx9_module = nullptr;
  decltype(D3DXCreateEffect)* D3DXCreateEffect = nullptr;
  decltype(D3DXFloat32To16Array)* D3DXFloat32To16Array = nullptr;

  HMODULE d3dcompiler_module = nullptr;

  ~d3d9_api()
  {
    if (d3d9_module) {
      FreeLibrary(d3d9_module);
    }
    if (d3dx9_module) {
      FreeLibrary(d3dx9_module);
    }
    if (d3dcompiler_module) {
      FreeLibrary(d3dcompiler_module);
    }
  }
};

namespace parameters {
extern std::string const background_color, foreground_color, highlight_color,
  selection_color, cursor_position, cursor_visible, seek_position, seeking,
  viewport_size, replaygain, orientation, flipped, shade_played, waveform_data,
  channel_magnitude, track_magnitude, track_time, track_duration, real_time;
};

struct effect_parameters
{
  struct attribute
  {
    enum Kind
    {
      FLOAT,
      BOOL,
      VECTOR4,
      MATRIX,
      TEXTURE
    } kind;
    union
    {
      float f;
      bool b;
      std::array<float, 4> v;
      std::array<float, 16> m;
      IDirect3DTexture9* t;
    };
  };
  std::map<std::string, attribute> attributes;

  void assign(attribute& dst, float f)
  {
    dst.kind = attribute::FLOAT;
    dst.f = f;
  }
  void assign(attribute& dst, bool b)
  {
    dst.kind = attribute::BOOL;
    dst.b = b;
  }
  void assign(attribute& dst, D3DXVECTOR4 v)
  {
    dst.kind = attribute::VECTOR4;
    memcpy(dst.v.data(), v, sizeof(float) * 4);
  }
  void assign(attribute& dst, D3DXMATRIX m)
  {
    dst.kind = attribute::MATRIX;
    memcpy(dst.m.data(), m, sizeof(float) * 16);
  }
  void assign(attribute& dst, IDirect3DTexture9* t)
  {
    dst.kind = attribute::TEXTURE;
    dst.t = t;
  }

  template<typename T>
  void set(std::string what, T const& t)
  {
    assign(attributes[what], t);
  }

  void apply_to(CComPtr<ID3DXEffect> fx);
};

struct frontend_impl : visual_frontend
{
  friend config_dialog;

  frontend_impl(HWND wnd,
                wave::size client_size,
                visual_frontend_callback& callback,
                visual_frontend_config& conf);
  virtual void clear();
  virtual void draw();
  virtual void present();
  virtual void on_state_changed(state s);
  virtual void show_configuration(HWND parent);
  virtual void close_configuration();
  int get_present_interval() const { return 10; } // milliseconds

  virtual void make_screenshot(screenshot_settings const* settings);

private: // Update
  bool draw_to_target(int target_width,
                      int target_height,
                      IDirect3DSurface9* render_target = NULL);
  void update_effect_colors();
  void update_effect_cursor();
  void update_replaygain();
  void update_data();
  void update_size();
  void update_orientation();
  void update_flipped();
  void update_shade_played();

private: // Misc state
  d3d9_api api;

  CComPtr<IDirect3D9> d3d;
  CComPtr<IDirect3DDevice9> dev;
  duration_query real_time;

  std::map<unsigned, CComPtr<IDirect3DTexture9>> channel_textures;
  std::vector<unsigned> channel_numbers;
  std::deque<channel_info> channel_order;

  std::stack<ref_ptr<effect_handle>> effect_stack;
  ref_ptr<effect_handle> effect_override;

  CComPtr<IDirect3DVertexBuffer9> vb;
  CComPtr<IDirect3DVertexDeclaration9> decl;

  effect_parameters effect_params;

  CComPtr<ID3DXEffect> select_effect();

  D3DPRESENT_PARAMETERS pp;

  std::map<unsigned, D3DXVECTOR4> channel_magnitudes;
  D3DXVECTOR4 track_magnitude;

private: // Host references
  visual_frontend_callback& callback;
  visual_frontend_config& conf;

private: // Resources
  CComPtr<IDirect3DTexture9> create_waveform_texture();
  void create_vertex_resources();
  void release_vertex_resources();
  void create_default_resources();
  void release_default_resources();

  bool device_still_lost();

  bool device_lost;
  UINT mip_count;
  D3DFORMAT texture_format;

private: // Configuration
  std::unique_ptr<config_dialog> config;

  void get_effect_compiler(ref_ptr<effect_compiler>& out);
  void set_effect(ref_ptr<effect_handle> effect, bool permanent);
};

struct config_dialog : CDialogImpl<config_dialog>
{
  enum
  {
    IDD = IDD_CONFIG_D3D
  };
  config_dialog(ref_ptr<frontend_impl> fe);

  BEGIN_MSG_MAP_EX(config_dialog)
  MSG_WM_INITDIALOG(on_wm_init_dialog)
  MSG_WM_CLOSE(on_wm_close)
  COMMAND_HANDLER_EX(IDC_EFFECT_APPLY, BN_CLICKED, on_effect_apply_click)
  COMMAND_HANDLER_EX(IDC_EFFECT_DEFAULT, BN_CLICKED, on_effect_default_click)
  COMMAND_HANDLER_EX(IDC_EFFECT_RESET, BN_CLICKED, on_effect_reset_click)
  NOTIFY_HANDLER_EX(IDC_EFFECT_SOURCE, SCN_MODIFIED, on_effect_source_modified)
  COMMAND_HANDLER_EX(9001, EN_CHANGE, on_effect_source_change)
  END_MSG_MAP()

private:
  LRESULT on_wm_init_dialog(CWindow focus, LPARAM lparam);
  void on_wm_close();
  void on_effect_apply_click(UINT, int, CWindow);
  void on_effect_default_click(UINT, int, CWindow);
  void on_effect_reset_click(UINT, int, CWindow);
  void on_effect_source_change(UINT, int, CEdit = 0);
  LRESULT on_effect_source_modified(NMHDR* hdr);

  virtual void OnFinalMessage(HWND);

  struct scintilla_func
  {
    int send(int cmd, int a1 = 0, int a2 = 0) const { return f(cmd, a1, a2); }

    template<class A1>
    int send(int cmd, A1 a1) const
    {
      return f(cmd, (int)a1);
    }

    template<class A1, class A2>
    int send(int cmd, A1 a1, A2 a2) const
    {
      return f(cmd, (int)a1, (int)a2);
    }

    void get_all(std::string& out)
    {
      out.clear();
      auto cb = send(SCI_GETTEXTLENGTH);
      if (!cb)
	return;

      out.resize(cb);
      send(SCI_GETTEXT, out.size() + 1, &out[0]);
    }

    void get_all(pfc::string8& out)
    {
      std::string tmp;
      get_all(tmp);
      out.reset();
      out.add_string(tmp.c_str(), tmp.size());
    }

    void reset(std::string const& text)
    {
      send(SCI_CLEARALL);
      send(EM_EMPTYUNDOBUFFER);
      send(SCI_ADDTEXT, text.size(), &text[0]);
      send(SCI_GOTOPOS, 0);
    }

    void reset(std::vector<char> const& text)
    {
      send(SCI_CLEARALL);
      send(EM_EMPTYUNDOBUFFER);
      send(SCI_ADDTEXT, text.size(), &text[0]);
      send(SCI_GOTOPOS, 0);
    }

    void clear_annotations()
    {
      annotations.clear();
      send(SCI_ANNOTATIONCLEARALL);
    }

    static void trim(std::string& in)
    {
      if (in.empty())
	return;
      auto c = in.c_str();
      size_t a = 0u;
      size_t b = in.size() - 1;
      while (isspace(in[b]) && b > a) {
	--b;
      }
      while (isspace(in[a]) && a < b) {
	++a;
      }
      in = std::string(c + a, b - a);
    }

    void add_annotation(int line, std::string text)
    {
      if (line < 0)
	return;
      trim(text);
      std::string& s = annotations[line];
      if (!s.empty())
	s += '\n';
      s += text;
      trim(s);
      send(SCI_ANNOTATIONSETTEXT, line, s.c_str());
      send(SCI_ANNOTATIONSETVISIBLE, 2);
    }

    std::map<int, std::string> annotations;

    void init(HWND wnd);
    std::function<int(int, int, int)> f;
  };

  ref_ptr<frontend_impl> fe;
  ref_ptr<effect_compiler> compiler;
  CFont mono_font;
  scintilla_func code_box;
  CEdit error_box;
  CButton apply_button, default_button, reset_button;
  ref_ptr<effect_handle> fx;
};

struct NOVTABLE effect_compiler : ref_base
{
  effect_compiler() {}
  effect_compiler(effect_compiler const&);
  effect_compiler& operator=(effect_compiler const&);
  struct diagnostic_entry
  {
    std::string line;
  };

  struct diagnostic_sink
  {
    virtual void on_error(char const* line) const abstract;
  };

  virtual ~effect_compiler() {}
  virtual bool compile_fragment(ref_ptr<effect_handle>& effect,
                                diagnostic_sink const& output,
                                char const* data,
                                size_t data_bytes) = 0;
};

struct NOVTABLE effect_handle : ref_base
{
  effect_handle() {}
  effect_handle(effect_handle const&);
  effect_handle& operator=(effect_handle const&);
  virtual ~effect_handle() {}
  virtual CComPtr<ID3DXEffect> get_effect() const = 0;
};

struct diagnostic_collector : effect_compiler::diagnostic_sink
{
  struct entry
  {
    std::string line;
  };

  void on_error(char const* line) const override
  {
    entry e = { line };
    entries.push_back(e);
  }

  diagnostic_collector(std::deque<entry>& entries)
    : entries(entries)
  {
    entries.clear();
  }

  std::deque<entry>& entries;
};
}
}
