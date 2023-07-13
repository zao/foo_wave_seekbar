//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <zlib.h>

namespace pack
{
	template <typename Iterator>
	bool z_pack(void const* src, size_t cb, Iterator I)
	{	
		z_stream zs = {};
		zs.zalloc = Z_NULL;
		zs.zfree = Z_NULL;
		zs.opaque = NULL;

		deflateInit(&zs, Z_DEFAULT_COMPRESSION);
		
		zs.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(src));
		zs.avail_in = (uInt)cb;
		
		std::vector<char> out_buf(deflateBound(&zs, (uLong)cb));
		zs.next_out = reinterpret_cast<Bytef*>(&out_buf[0]);
		zs.avail_out = (uInt)out_buf.size();

		deflate(&zs, Z_FINISH);
		deflateEnd(&zs);

		std::copy_n(out_buf.begin(), zs.total_out, I);
		return true;
	}

	template <typename Iterator>
	bool z_unpack(void const* src, size_t cb, Iterator I)
	{
		z_stream zs = {};
		zs.zalloc = Z_NULL;
		zs.zfree = Z_NULL;
		zs.opaque = NULL;

		zs.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(src));
		zs.avail_in = (uInt)cb;

		inflateInit(&zs);

		size_t const OUT_CB = 1024;
		char buf[OUT_CB];
		zs.next_out = reinterpret_cast<Bytef*>(buf);
		zs.avail_out = OUT_CB;
		
		while (1)
		{
			int res = inflate(&zs, Z_NO_FLUSH);
			if (res < 0)
				return false;
			if (zs.avail_out != OUT_CB)
			{
				std::copy(buf, buf + OUT_CB - zs.avail_out, I);
				zs.next_out = reinterpret_cast<Bytef*>(buf);
				zs.avail_out = OUT_CB;
			}
			if (res == Z_STREAM_END)
				break;
		}
		inflateEnd(&zs);
		return true;
	}
}
