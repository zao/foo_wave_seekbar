#pragma once
#include "../../../pfc/list.h"

template <typename T>
float downmix(pfc::list_t<T> const& frame)
{
	const T sqrt_half = T(0.70710678118654752440084436210485);
	pfc::list_t<T> data = frame;
	switch (data.get_size())
	{
	case 8:
		data[0] += frame[6] * sqrt_half;
		data[1] += frame[7] * sqrt_half;
	case 6:
		data[0] += frame[2] * sqrt_half + frame[4] * sqrt_half + frame[3];
		data[1] += frame[2] * sqrt_half + frame[5] * sqrt_half + frame[3];
	case 2:
		data[0] += frame[1];
		data[0] /= T(2.0);
		break;
	case 4:
		data[0] += frame[1] + frame[2] + frame[3];
		data[0] /= T(4.0);
	}
	return data[0];
}