#pragma once

struct remember_pointers
{
	template <typename P>
	P remember(P p)
	{
		pointers.push_back(p);
		return p;
	}

	std::deque<shared_ptr<void>> pointers;
};