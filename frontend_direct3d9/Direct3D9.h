//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../frontend_sdk/VisualFrontend.h"
#include <map>
#include "resource.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/fusion/include/adapted.hpp>
#include <boost/optional.hpp>
#include "Scintilla.h"

namespace wave
{
  bool has_direct3d9();

  struct duration_query
  {
    LARGE_INTEGER then;
    duration_query()
    {
        QueryPerformanceCounter(&then);
    }

    double get_elapsed() const
    {
      LARGE_INTEGER now, freq;
      QueryPerformanceCounter(&now);
      QueryPerformanceFrequency(&freq);
      return (double)(now.QuadPart - then.QuadPart) / (double)freq.QuadPart;
    }
  };

  namespace direct3d9
  {
    struct config_dialog;
    struct effect_compiler;
    struct effect_handle;

    extern const GUID guid_fx_string;

    namespace parameters
    {
      extern std::string const background_color, foreground_color, highlight_color, selection_color,
        cursor_position, cursor_visible,
        seek_position, seeking,
        viewport_size, replaygain,
        orientation, flipped, shade_played,
        waveform_data, channel_magnitude, track_magnitude,
		track_time, track_duration, real_time;
    };

    struct effect_parameters
    {
      typedef boost::variant<float, bool, D3DXVECTOR4, D3DXMATRIX, IDirect3DTexture9*> attribute;
      std::map<std::string, attribute> attributes;

      template <typename T>
      void set(std::string what, T const& t)
      {
        attributes[what] = t;
      }

      void apply_to(CComPtr<ID3DXEffect> fx);
    };

    struct frontend_impl : visual_frontend
    {
      friend config_dialog;

      frontend_impl(HWND wnd, wave::size client_size, visual_frontend_callback& callback, visual_frontend_config& conf);
      virtual void clear();
      virtual void draw();
      virtual void present();
      virtual void on_state_changed(state s);
      virtual void show_configuration(HWND parent);
      virtual void close_configuration();
      int get_present_interval() const { return 10; } // milliseconds

      virtual void make_screenshot(screenshot_settings const* settings);

    private: // Update
      bool draw_to_target(int target_width, int target_height, IDirect3DSurface9* render_target = NULL);
      void update_effect_colors();
      void update_effect_cursor();
      void update_replaygain();
      void update_data();
      void update_size();
      void update_orientation();
      void update_flipped();
      void update_shade_played();

    private: // Misc state
      CComPtr<IDirect3D9> d3d;
      CComPtr<IDirect3DDevice9> dev;
	  duration_query real_time;

      std::map<unsigned, CComPtr<IDirect3DTexture9>> channel_textures;
      std::vector<unsigned> channel_numbers;
      std::vector<channel_info> channel_order;

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
      bool floating_point_texture;

    private: // Configuration
      scoped_ptr<config_dialog> config;

      void get_effect_compiler(ref_ptr<effect_compiler>& out);
      void set_effect(ref_ptr<effect_handle> effect, bool permanent);
    };

    struct config_dialog : CDialogImpl<config_dialog>
    {
      enum { IDD = IDD_CONFIG_D3D };
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
        int send (int cmd, int a1 = 0, int a2 = 0) const { return f(cmd, a1, a2); }
        
        template <class A1>
        int send (int cmd, A1 a1) const { return f(cmd, (int)a1); }

        template <class A1, class A2>
        int send (int cmd, A1 a1, A2 a2) const { return f(cmd, (int)a1, (int)a2); }

        void get_all(std::string& out)
        {
          out.clear();
          auto cb = send(SCI_GETTEXTLENGTH);
          if (!cb)
            return;

          out.resize(cb);
          send(SCI_GETTEXT, out.size()+1, &out[0]);
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

        void add_annotation(int line, std::string text)
        {
          if (line < 0)
            return;
          boost::algorithm::trim(text);
          std::string& s = annotations[line];
          if (!s.empty())
            s += '\n';
          s += text;
          boost::algorithm::trim(s);
          send(SCI_ANNOTATIONSETTEXT, line, s.c_str());
          send(SCI_ANNOTATIONSETVISIBLE, 2);
        }

        std::map<int, std::string> annotations;

        void init(HWND wnd);
        std::function<int (int, int, int)> f;
      };

      ref_ptr<frontend_impl> fe;
      ref_ptr<effect_compiler> compiler;
      CFont mono_font;
      scintilla_func code_box;
      CEdit error_box;
      CButton apply_button, default_button, reset_button;
      ref_ptr<effect_handle> fx;
    };

    struct NOVTABLE effect_compiler : ref_base, noncopyable
    {
      struct diagnostic_entry
      {
        struct location
        {
          int row, col;
        };

        boost::optional<location> loc;
        std::string type, code, message;
      };

			struct diagnostic_sink
			{
				virtual void on_error(char const* type, char const* code, char const* message) const abstract;
				virtual void on_error(char const* type, char const* code, char const* message, int row, int col) const abstract;
			};

      virtual ~effect_compiler() {}
      virtual bool compile_fragment(ref_ptr<effect_handle>& effect, diagnostic_sink const& output, char const* data, size_t data_bytes) = 0;
    };

    struct NOVTABLE effect_handle : ref_base, noncopyable
    {
      virtual ~effect_handle() {}
      virtual CComPtr<ID3DXEffect> get_effect() const = 0;
    };
	
		struct diagnostic_collector : effect_compiler::diagnostic_sink
		{
			struct entry
			{
				int row, col;
				bool has_loc;
				std::string type, code, message;
			};

			void on_error(char const* type, char const* code, char const* message, int row, int col) const override
			{
				entry e = { row, col, true, type, code, message };
				entries.push_back(e);
			}

			void on_error(char const* type, char const* code, char const* message) const override
			{
				on_error(type, code, message, 0, 0);
				entries.back().has_loc = false;
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

BOOST_FUSION_ADAPT_STRUCT(
  wave::direct3d9::effect_compiler::diagnostic_entry::location,
  (int, row)
  (int, col)
  )

BOOST_FUSION_ADAPT_STRUCT(
  wave::direct3d9::effect_compiler::diagnostic_entry,
  (boost::optional<wave::direct3d9::effect_compiler::diagnostic_entry::location>, loc)
  (std::string, type)
  (std::string, code)
  (std::string, message)
  )
