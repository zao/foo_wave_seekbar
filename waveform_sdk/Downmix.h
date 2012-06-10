//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../../../pfc/list.h"

template <typename T>
float downmix(pfc::list_t<T> frame)
{
	const T sqrt_half = T(0.70710678118654752440084436210485);
	t_size n_ch = frame.get_size();
	switch (n_ch)
	{
	case 8:
		frame[0] += frame[6] * sqrt_half;
		frame[1] += frame[7] * sqrt_half;
	case 6:
		frame[0] += frame[2] * sqrt_half + frame[4] * sqrt_half + frame[3];
		frame[1] += frame[2] * sqrt_half + frame[5] * sqrt_half + frame[3];
	case 2:
		frame[0] += frame[1];
		frame[0] /= T(2.0);
		break;
	case 4:
		frame[0] += frame[1] + frame[2] + frame[3];
		frame[0] /= T(4.0);
		break;
	default:
		for (t_size i = 1; i < n_ch; ++i)
		{
			frame[0] += frame[i];
		}
		frame[0] /= n_ch;
	}
	return frame[0];
}
