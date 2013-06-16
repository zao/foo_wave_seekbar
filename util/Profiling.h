//          Copyright Lars Viklund 2008 - 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <string>
#include <unordered_map>

namespace util
{
enum class Phase
{
	BEGIN_EVENT = 'B', END_EVENT = 'E',
	INSTANT = 'I',
	ASYNC_START = 'S', ASYNC_FINISH = 'F',
	COUNTER = 'C',
};

typedef std::unordered_map<std::string, std::string> EventArgs;

bool is_recording_enabled();
void record_event(Phase phase, char const* category, char const* name, uint64_t const* id = nullptr,
	EventArgs const* args = nullptr);
void record_value(char const* category, char const* name, double d);
uint64_t generate_recording_id();

template <Phase FromPhase, Phase ToPhase>
struct EventScope
{
	EventScope(char const* category, char const* name, EventArgs const* args = nullptr)
		: _category(category), _name(name)
		, _id(generate_recording_id())
	{
		record_event(FromPhase, category, name, &_id, args);
	}
	~EventScope()
	{
		record_event(ToPhase, _category, _name, &_id, nullptr);
	}

private:
	EventScope(EventScope const&);
	EventScope& operator = (EventScope const&);
	char const* _category;
	char const* _name;
	uint64_t _id;
};

typedef EventScope<Phase::BEGIN_EVENT, Phase::END_EVENT> ScopedEvent;
typedef EventScope<Phase::ASYNC_START, Phase::ASYNC_FINISH> AsyncEvent;
}