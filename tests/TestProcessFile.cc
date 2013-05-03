#include "tests/Testing.h"
#if SEEKBAR_TESTING
#include "CacheImpl.h"
#include "../../SDK/foobar2000.h"
#include "Helpers.h"

namespace tests
{
	void ensure_equal_duration(metadb_handle_list_cref p_data)
	{
		metadb_handle_list elems = p_data;
		in_main_thread(
			[elems]
			{
				console::formatter() << "Verifying equal duration for "
					<< elems.get_count() << " tracks...";

				std::map<std::tuple<t_int64, t_int64>, std::list<playable_location_impl>> track_durations;
				abort_callback_impl abort_cb;
				auto f = [&](metadb_handle_ptr p)
				{
					auto const& loc = p->get_location();
					if (!input_entry::g_is_supported_path(loc.get_path()))
						return;
						
					service_ptr_t<input_decoder> decoder;
					input_entry::g_open_for_decoding(decoder, 0, loc.get_path(), abort_cb);
					t_uint32 subsong = loc.get_subsong();

					decoder->initialize(subsong, input_flag_simpledecode, abort_cb);
					if (!decoder->can_seek())
						return;

					t_int64 sample_rate = 0;
					t_int64 sample_count = 0;
					if (!wave::try_determine_song_parameters(decoder, subsong, sample_rate, sample_count, abort_cb))
						return;

					track_durations[std::tie(sample_count, sample_rate)].push_back(loc);
				};				
				elems.enumerate(f);
				for (auto e : track_durations)
				{
					t_int64 sample_count, sample_rate;
					std::tie(sample_count, sample_rate) = e.first;
					console::formatter() << "Tracks with sample count " << sample_count << " ("
						<< ((double)sample_count / sample_rate) << " seconds):";
					for (auto loc : e.second)
					{
						console::formatter() << "  " << loc;
					}
				}
				console::formatter() << "Done verifying durations.";
			});
	}

	// {41D5D5DF-D6D0-4095-B43C-6BF5214CE649}
	static const GUID guid_ensure_equal_duration = 
	{ 0x41d5d5df, 0xd6d0, 0x4095, { 0xb4, 0x3c, 0x6b, 0xf5, 0x21, 0x4c, 0xe6, 0x49 } };

	DECLARE_CONTEXT_MENU_ITEM(report_chunk_lengths_context, "Verify equal durations", "Seekbar tests",
		ensure_equal_duration, guid_ensure_equal_duration, "Decodes tracks and verifies that their durations match.");
}
#endif