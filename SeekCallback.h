#pragma once

namespace wave
{
	struct seek_callback
	{
		virtual ~seek_callback() {}
		virtual void on_seek_begin() abstract;
		virtual void on_seek_position(double time, bool legal) abstract;
		virtual void on_seek_end(bool aborted) abstract;
	};
}