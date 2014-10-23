//          Copyright Lars Viklund 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <Windows.h>
#include <cstdint>
#include <deque>
#include <iostream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/error/en.h>
#include <json/json.h>
#include <json/jsoncpp.cpp>
#include <boost/atomic.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread.hpp>

namespace rj = rapidjson;

static boost::mutex m;
static std::deque<std::shared_ptr<rj::Document>> incoming;

static boost::condition_variable outgoing_cv;
static std::deque<Json::Value> outgoing;

static boost::atomic<bool> terminating;

enum { ZWM_INCOMING = WM_APP + 1 };

static DWORD main_thread_id = ~0;

static void read_thread_main() {
	size_t const READ_BUF_SIZE = 65536;
	char read_buf[READ_BUF_SIZE];
	rj::FileReadStream is(stdin, read_buf, READ_BUF_SIZE);
	while (1) {
		auto d = std::make_shared<rj::Document>();
		d->ParseStream<rj::kParseStopWhenDoneFlag>(is);
		if (d->HasParseError()) {
			auto ec = d->GetParseError();
			if (ec != rj::kParseErrorDocumentEmpty) {
				MessageBoxA(NULL, rj::GetParseError_En(ec), "has parse error", MB_OK);
				break;
			}
		}
		if (feof(stdin)) {
			break;
		}
		boost::unique_lock<boost::mutex> lk(m);
		incoming.push_back(d);
		PostThreadMessage(main_thread_id, ZWM_INCOMING, 0, 0);
	}
	PostQuitMessage(0);
}

static void write_thread_main() {
	boost::unique_lock<boost::mutex> lk(m);
	while (1) {
		while (outgoing.empty() && !terminating) {
			outgoing_cv.wait(lk);
		}
		if (terminating)
			return;
		Json::FastWriter w;
		for (auto I = outgoing.begin(); I != outgoing.end(); ++I) {
			w.write(*I);
		}
		outgoing.clear();
	}
}

int WINAPI WinMain(HINSTANCE my_instance, HINSTANCE prev_instance, LPSTR command_line, int show_command) {
	terminating = false;
	main_thread_id = GetCurrentThreadId();
	int argc;
	std::deque<std::wstring> argv;
	{
		wchar_t** argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
		for (int i = 0; i < argc; ++i) {
			argv.push_back(argv_w[i]);
		}
		LocalFree(argv_w);
	}
	if (false && argc == 1) {
		MessageBox(nullptr, L"This is not a standalone application.", L"foo_wave_seekbar D3D9 Shader Editor", MB_OK);
		return 1;
	}

	HWND b = CreateWindow(L"BUTTON", L"Moo.", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 64, 64, nullptr, nullptr, my_instance, 0);
	ShowWindow(b, SW_SHOWNORMAL);

	boost::thread read_thread(&read_thread_main);
	boost::thread write_thread(&write_thread_main);

	while (!terminating) {
		MSG msg;
		while (0 <= GetMessage(&msg, 0, 0, 0)) {
			if (msg.hwnd == 0) {
				switch (msg.message) {
				case ZWM_INCOMING: {
					boost::unique_lock<boost::mutex> lk(m);
					incoming.pop_front();
					MessageBox(nullptr, L"message arrived", L"moo!", MB_OK);
					break;
				}
				case WM_QUIT: {
					boost::unique_lock<boost::mutex> lk(m);
					terminating = true;
					outgoing_cv.notify_all();
					break;
				}
				}
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.hwnd == b && msg.message == WM_DESTROY) {
					PostQuitMessage(0);
				}
			}
		}
	}

	return 0;
}