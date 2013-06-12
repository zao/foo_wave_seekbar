#include "PchSeekbar.h"
#include "Profiling.h"
#include <mutex>
#include <tbb/atomic.h>
#include "json/json.h"
#include <ctime>
#include <fstream>
#include <cstdint>
#include "util/Asio.h"

namespace util
{
static std::string target_filename;
static tbb::atomic<bool> initialized, logging;
static std::mutex logging_mutex;
static boost::asio::io_service io;
static boost::asio::ip::tcp::socket log_socket(io);
static LARGE_INTEGER start_time_counter;

static double now()
{
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return (counter.QuadPart - start_time_counter.QuadPart) / (double)frequency.QuadPart;
}

static wchar_t const* const LOG_SERVER_FILE_MAPPING = L"LOG_SERVER_TRACE_PORT_MAPPING";
static size_t const LOG_SERVER_MAPPING_SIZE = 4u;

static void lazy_recording_init(std::lock_guard<std::mutex> const&)
{
	if (!initialized) {
		auto shared_mapping = OpenFileMapping(FILE_MAP_READ, FALSE, LOG_SERVER_FILE_MAPPING);
		if (shared_mapping) {
			auto p = (LONG*)MapViewOfFile(shared_mapping, FILE_MAP_READ, 0, 0, LOG_SERVER_MAPPING_SIZE);
			uint16_t port = 0u;
			while (port == 0)
				port = (uint16_t)*p;
			UnmapViewOfFile(p);
			CloseHandle(shared_mapping);
			boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4::loopback(), port);
			boost::system::error_code ec;
			log_socket.connect(endpoint, ec);
			uint8_t handshake[] = { 0u };
			boost::asio::write(log_socket, boost::asio::buffer(handshake, 1), ec);
			if (!ec) {
				logging = true;
				QueryPerformanceCounter(&start_time_counter);
				console::printf("Seekbar: Logging traces to port %d.\n", port);
			}
		}
		initialized = true;
	}
}

bool is_recording_enabled()
{
	if (!initialized) {
		std::lock_guard<std::mutex> lock(logging_mutex);
		lazy_recording_init(lock);
	}
	return logging;
}

static tbb::atomic<uint64_t> id_source;
uint64_t generate_recording_id()
{
	return id_source++;
}

void record_event(Phase phase, char const* category, char const* name, uint64_t const* id,
	EventArgs const* args)
{
	std::lock_guard<std::mutex> lock(logging_mutex);
	if (!initialized) {
		lazy_recording_init(lock);
	}
	if (logging) {
		Json::Value event_node;
		char phase_string[2] = { (char)phase };
		event_node["cat"] = category;
		event_node["name"] = name;
		event_node["pid"] = 0;
		event_node["tid"] = (Json::UInt)GetCurrentThreadId();
		event_node["ts"] = now() * 1000000.0;
		event_node["ph"] = phase_string;
		if (id) {
			std::ostringstream oss;
			oss << "0x" << std::hex << *id;
			event_node["id"] = oss.str();
		}
		if (args) {
			auto& node = event_node["args"] = Json::Value(Json::objectValue);
			for (auto& arg : *args)
				node[arg.first] = arg.second;
		}
		auto s = event_node.toStyledString();
		if (s.size() > 0xFFFFu) {
			console::warning("Seekbar: Oversized trace message, dropping it.");
			return;
		}
		boost::system::error_code ec;
		uint8_t op = 0u; // OP_ENTRY
		uint16_t num_bytes = htons((uint16_t)s.size());
		boost::asio::write(log_socket, boost::asio::buffer(&op, 1), ec);
		boost::asio::write(log_socket, boost::asio::buffer(&num_bytes, 2), ec);
		boost::asio::write(log_socket, boost::asio::buffer(s), ec);
	}
}
}