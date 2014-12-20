//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include "resource.h"
#include "microhttpd.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <json/json.h>
#include <json/jsoncpp.cpp>

namespace wave
{
	template <typename T>
	inline T lerp(T a, T b, float n)
	{
		return (1.0f - n)*a + n*b;
	}

	struct create_d3d9_func
	{
		IDirect3D9* operator() () const
		{
			IDirect3D9* p = Direct3DCreate9(D3D_SDK_VERSION);
			return p;
		}
	};

	struct test_d3dx9_func
	{
		bool operator() () const
		{
			D3DXGetDriverLevel(nullptr);
			return true;
		}
	};

	namespace direct3d9
	{
		frontend_impl::frontend_impl(HWND wnd, wave::size client_size, visual_frontend_callback& callback, visual_frontend_config& conf)
			: mip_count(4), callback(callback), conf(conf), floating_point_texture(true)
		{
			HRESULT hr = S_OK;

			d3d.Attach(create_d3d9_func()());

			if (!d3d)
			{
				throw std::runtime_error("DirectX redistributable not found. Run the DirectX February 2010 web setup or later.");
			}

			ZeroMemory(&pp, sizeof(pp));
			pp.BackBufferWidth = client_size.cx;
			pp.BackBufferHeight = client_size.cy;
			pp.BackBufferFormat = D3DFMT_A8R8G8B8;
			pp.BackBufferCount = 1;
			pp.MultiSampleType = D3DMULTISAMPLE_NONE;
			pp.MultiSampleQuality = 0;
			pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			pp.hDeviceWindow = wnd;
			pp.Windowed = TRUE;
			pp.EnableAutoDepthStencil = FALSE;
			pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
			pp.Flags = 0;
			pp.FullScreen_RefreshRateInHz = 0;
			pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

			DWORD msaa_quality = 0;
			for (INT x = D3DMULTISAMPLE_16_SAMPLES; x >= 0; --x)
			{
				D3DMULTISAMPLE_TYPE msaa_type = (D3DMULTISAMPLE_TYPE)x;
				if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, TRUE, msaa_type, &msaa_quality)))
				{
					pp.MultiSampleType = msaa_type;
					pp.MultiSampleQuality = msaa_quality - 1;
					break;
				}
			}

			hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, wnd, D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
			if (!SUCCEEDED(hr))
			{
				pp.MultiSampleType = D3DMULTISAMPLE_NONE;
				pp.MultiSampleQuality = 0;
				hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, wnd, D3DCREATE_FPU_PRESERVE | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
				if (!SUCCEEDED(hr))
					throw std::exception("Direct3D9: could not create device.");
			}

			auto query_texture_format = [this](D3DFORMAT fmt) -> bool
			{
				CComPtr<IDirect3DTexture9> tex;
				HRESULT hr = dev->CreateTexture(2048, 1, mip_count, 0, fmt, D3DPOOL_MANAGED, &tex, 0);
				return SUCCEEDED(hr);
			};
		
			//if (!query_texture_format(texture_format = D3DFMT_A16B16G16R16F))
			{
				floating_point_texture = false;
				//if (!query_texture_format(texture_format = D3DFMT_A2R10G10B10))
				{
					if (!query_texture_format(texture_format = D3DFMT_A8R8G8B8))
						throw std::exception("Direct3D9: could not find a suitable texture format.");
				}
			}

			create_vertex_resources();
			create_default_resources();
		}

		void frontend_impl::clear()
		{
			if (device_still_lost())
				return;
			color c = callback.get_color(config::color_background);
			D3DXCOLOR bg(c.r, c.g, c.b, c.a);
			dev->Clear(0, 0, D3DCLEAR_TARGET, bg, 1.0f, 0);
		}

		void frontend_impl::draw()
		{
			draw_to_target(pp.BackBufferWidth, pp.BackBufferHeight, NULL);
		}

		bool frontend_impl::draw_to_target(int target_width, int target_height, IDirect3DSurface9* render_target)
		{
			if (device_still_lost())
				return false;

			dev->SetRenderTarget(0, render_target);
			D3DVIEWPORT9 window_viewport;
			dev->GetViewport(&window_viewport);

			auto draw_quad = [&,this](int idx, int ch, int n)
			{
				auto channel_viewport = window_viewport;
				D3DXVECTOR2 sides((float)n - idx - 1, (float)n - idx);
				D3DXVECTOR4 viewport = D3DXVECTOR4((float)target_width, (float)target_height, 0.0f, 0.0f);
				sides /= (float)n;

				std::vector<float> buf;

				if (callback.get_orientation() == config::orientation_horizontal)
				{
					channel_viewport.Y = (int)(sides[0]*target_height);
					float h = (sides[1]-sides[0])*target_height;
					channel_viewport.Height = (int)h;
					viewport.y = floor(h);
					float arr[] = {
						// position2f, texcoord2f
						-1.0f, -1.0f, -1.0f, -1.0f,
						-1.0f,  3.0f, -1.0f,  3.0f,
						 3.0f, -1.0f,  3.0f, -1.0f,
					};
					buf.insert(buf.end(), std::begin(arr), std::end(arr));
				}
				else
				{
					channel_viewport.X = (int)(sides[0]*target_width);
					float w = (sides[1]-sides[0])*target_width;
					channel_viewport.Width = (int)w;
					viewport.x = floor(w);
					float arr[] = {
						-1.0f, -1.0f,  1.0f, -1.0f,
						-1.0f,  3.0f,  1.0f,  3.0f,
						 3.0f, -1.0f, -3.0f, -1.0f,
					};
					buf.insert(buf.end(), std::begin(arr), std::end(arr));
				}

				effect_params.set(parameters::viewport_size, viewport);

				HRESULT hr = S_OK;
				hr = dev->BeginScene();
				effect_params.set(parameters::waveform_data, channel_textures[ch]);
				effect_params.set(parameters::channel_magnitude, channel_magnitudes[ch]);
				effect_params.set(parameters::track_magnitude, track_magnitude);
				effect_params.set(parameters::track_time, (float)callback.get_playback_position());
				effect_params.set(parameters::track_duration, (float)callback.get_track_length());
				effect_params.set(parameters::real_time, (float)real_time.get_elapsed());

				CComPtr<ID3DXEffect> fx = select_effect();
				effect_params.apply_to(fx);

				hr = dev->SetTexture(0, channel_textures[ch]);
				hr = dev->SetVertexDeclaration(decl);
				hr = dev->SetViewport(&channel_viewport);

				UINT passes = 0;
				fx->Begin(&passes, 0);
				for (UINT pass = 0; pass < passes; ++pass)
				{
					fx->BeginPass(pass);
					fx->CommitChanges();
					hr = dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 1, &buf[0], sizeof(float) * 4);
					fx->EndPass();
				}
				fx->End();
				hr = dev->EndScene();
			};

			size_t num = channel_order.size();
			auto I = channel_order.begin();
			for (size_t idx = 0; idx < num; ++idx, ++I)
			{
				draw_quad(idx, I->channel, num);
			}
			dev->SetViewport(&window_viewport);
			return true;
		}

		void frontend_impl::present()
		{
			HRESULT hr  = dev->Present(0, 0, 0, 0);
			if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET)
			{
				device_lost = true;
				update_size();
			}
		}

		CComPtr<ID3DXEffect> frontend_impl::select_effect()
		{
			if (effect_override)
				return effect_override->get_effect();
			return effect_stack.top()->get_effect();
		}

		void frontend_impl::on_state_changed(state s)
		{
			if (device_still_lost())
				return;
			if (s & state_size)
				update_size();
			if (s & state_color)
				update_effect_colors();
			if (s & state_position)
				update_effect_cursor();
			if (s & state_replaygain)
				update_replaygain();
			if (s & (state_data | state_channel_order | state_downmix_display))
				update_data();
			if (s & state_orientation)
				update_orientation();
			if (s & state_flip_display)
				update_flipped();
			if (s & state_shade_played)
				update_shade_played();
			CWindow wnd = this->pp.hDeviceWindow;
			wnd.Invalidate(FALSE);
		}

		bool frontend_impl::device_still_lost()
		{
			if (device_lost)
				update_size();
			return device_lost;
		}

		bool slurp_component_file(wchar_t const* filename, std::vector<char>& out)
		{
			auto root = get_component_directory();
			root += filename;
			HANDLE f = CreateFile(root.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
			if (!f) {
				out.clear();
				return false;
			}
			LARGE_INTEGER sz = {};
			GetFileSizeEx(f, &sz);
			if (sz.HighPart) {
				CloseHandle(f);
				out.clear();
				return false;
			}
			out.resize(sz.LowPart);
			DWORD num_read = 0;
			ReadFile(f, out.data(), out.size(), &num_read, 0);
			CloseHandle(f);
			if (num_read != sz.LowPart) {
				out.clear();
				return false;
			}
			return true;
		}

		bool slurp_webroot_file(wchar_t const* webroot, char const* url, std::vector<char>& out)
		{
			std::wstring filepath = webroot;
			if (filepath.back() != L'\\') {
				filepath += L"\\";
			}
			std::wstring s(url + 1, url + strlen(url));
			for (auto I = s.begin(); I != s.end(); ++I) {
				if (*I == L'/') *I = L'\\';
			}
			filepath += s;
			return slurp_component_file(filepath.c_str(), out);
		}

		struct config_handler
		{
			static int on_web_connection(void* cls, MHD_Connection* connection, char const* url,
				char const* method, char const* version, char const* upload_data, size_t* upload_data_size,
				void** conn_cls)
			{
				frontend_impl* self = (frontend_impl*)cls;
				MHD_Response* response;
				int ret = MHD_NO;
				if (strcmp(url, "/") == 0) {
					url = "/index.html";
				}
				if (strcmp(url, "/data") == 0) {
					Json::Value root;
					if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
						std::string stored_body;
						self->conf.get_configuration_string(guid_fx_string, std_string_sink(stored_body));
						root["effect"] = stored_body;
						Json::FastWriter w;
						auto data = w.write(root);
						response = MHD_create_response_from_buffer(data.size(), (void*)data.data(), MHD_RESPMEM_MUST_COPY);
						MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json");
						ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
						MHD_destroy_response(response);
					}
					else if (strcmp(method, MHD_HTTP_METHOD_PUT) == 0) {
						if (!*conn_cls) {
							*conn_cls = (void*)1;
							return MHD_YES;
						}
						else {
							Json::Reader r;
							if (r.parse(upload_data, upload_data + *upload_data_size, root) &&
								root.isMember("effect") && root["effect"].isString()) {
								std::string effect_body = root["effect"].asString();
								ref_ptr<effect_compiler> ec;
								self->get_effect_compiler(ec);
								ref_ptr<effect_handle> fx;
								std::deque<diagnostic_collector::entry> output;
								ec->compile_fragment(fx, diagnostic_collector(output), effect_body.data(), effect_body.size());
								self->set_effect(fx, false);
								response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
								ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
								MHD_destroy_response(response);
								*upload_data_size = 0;
							}
							else {
								response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
								ret = MHD_queue_response(connection, MHD_HTTP_CONFLICT, response);
								MHD_destroy_response(response);
							}
						}
					}
					else {
						response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
						ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
						MHD_destroy_response(response);
					}
				}
				else if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
					std::vector<char> data;
					if (slurp_webroot_file(L"editor", url, data)) {
						response = MHD_create_response_from_buffer(data.size(), data.data(), MHD_RESPMEM_MUST_COPY);
						if (boost::iends_with(std::string(url), ".js")) {
							MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/javascript");
						}
						else if (boost::iends_with(std::string(url), ".html") ||
							boost::iends_with(std::string(url), ".htm")) {
							MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
						}
						ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
						MHD_destroy_response(response);
					}
					else {
						response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
						ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
						MHD_destroy_response(response);
					}
				}
				else {
					response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
					ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
					MHD_destroy_response(response);
				}
				return ret;
			}
		};

		static MHD_Daemon* web_server;

		void frontend_impl::show_configuration(HWND parent)
		{
			if (config)
				config->BringWindowToTop();
			else
			{
				ref_ptr<frontend_impl> p(this);
				config.reset(new config_dialog(p));
				config->Create(parent);
			}
			if (!web_server) {
				web_server = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 9001, 0, 0,
					&config_handler::on_web_connection, (void*)this, MHD_OPTION_END);
			}
			ShellExecuteA(0, "open", "http://localhost:9001/", 0, 0, SW_SHOWNORMAL);
		}

		void frontend_impl::close_configuration()
		{
			if (web_server) {
				MHD_stop_daemon(web_server);
			}
			if (config)
			{
				config->DestroyWindow();
				config.reset();
			}
		}

		void frontend_impl::make_screenshot(screenshot_settings const* settings)
		{
			CComPtr<IDirect3DSurface9> rt;
			HRESULT hr = dev->CreateRenderTarget(settings->width, settings->height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &rt, nullptr);
			if (FAILED(hr))
				return;

			IDirect3DSurface9* old_rt = nullptr;
			dev->GetRenderTarget(0, &old_rt);
			if (draw_to_target(settings->width, settings->height, rt))
			{
				D3DLOCKED_RECT r = {};
				rt->LockRect(&r, NULL, 0);
				settings->write_screenshot(settings->context, (BYTE*)r.pBits);
			}
			dev->SetRenderTarget(0, old_rt);
		}

		void frontend_impl::get_effect_compiler(ref_ptr<effect_compiler>& out)
		{
			out.reset(new effect_compiler_impl(dev));
		}

		void frontend_impl::set_effect(ref_ptr<effect_handle> in, bool permanent)
		{
			if (permanent)
			{
				if (effect_stack.size() > 1) // TODO: dynamic count for the future?
					effect_stack.pop();
				effect_stack.push(in);
			}
			else
				effect_override = in;
		}

		namespace parameters
		{
			std::string const
				background_color = "BACKGROUNDCOLOR",
				foreground_color = "TEXTCOLOR",
				highlight_color = "HIGHLIGHTCOLOR",
				selection_color = "SELECTIONCOLOR",
			
				cursor_position = "CURSORPOSITION",
				cursor_visible = "CURSORVISIBLE",
			
				seek_position = "SEEKPOSITION",
				seeking = "SEEKING",

				viewport_size = "VIEWPORTSIZE",
				replaygain = "REPLAYGAIN",

				orientation = "ORIENTATION",
				flipped = "FLIPPED",
				shade_played = "SHADEPLAYED",
			
				waveform_data = "WAVEFORMDATA",

				channel_magnitude = "CHANNELMAGNITUDE",
				track_magnitude = "TRACKMAGNITUDE",

				track_time = "TRACKTIME",
				track_duration = "TRACKDURATION",
				real_time = "REALTIME";
		}

		struct attribute_setter
		{
			CComPtr<ID3DXEffect> const& fx;
			std::string const& key;
			explicit attribute_setter(CComPtr<ID3DXEffect> const& fx, std::string const& key) : fx(fx), key(key) {}

			D3DXHANDLE get() const { return fx->GetParameterBySemantic(nullptr, key.c_str()); }
			typedef effect_parameters::attribute attribute;

			void operator () (attribute const& attr) const
			{
				switch (attr.kind) {
					case attribute::FLOAT:   apply(attr.f); break;
					case attribute::BOOL:    apply(attr.b); break;
					case attribute::VECTOR4: apply(attr.v); break;
					case attribute::MATRIX:  apply(attr.m); break;
					case attribute::TEXTURE: apply(attr.t); break;
				}
			}

			// <float, bool, D3DXVECTOR4, D3DXMATRIX, IDirect3DTexture9>
			void apply(float const& f) const
			{
				if (auto h = get()) fx->SetFloat(h, f);
			}

			void apply(bool const& b) const
			{
				if (auto h = get()) fx->SetBool(h, b);
			}

			void apply(std::array<float, 4> const& a) const
			{
				auto v = D3DXVECTOR4(a.data());
				if (auto h = get()) fx->SetVector(h, &v);
			}

			void apply(std::array<float, 16> const& a) const
			{
				auto m = D3DXMATRIX(a.data());
				if (auto h = get()) fx->SetMatrix(h, &m);
			}

			void apply(IDirect3DTexture9* tex) const
			{
				if (auto h = get()) fx->SetTexture(h, tex);
			}
		};

		void effect_parameters::apply_to(CComPtr<ID3DXEffect> fx)
		{
			std::for_each(attributes.begin(), attributes.end(),
				[fx](std::pair<std::string const, attribute>& value)
				{
					attribute_setter vtor(fx, value.first);
					vtor(value.second);
				});
		}
	}
}
