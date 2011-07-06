#pragma once
#include "zlib/zlib.h"

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
		zs.avail_in = cb;
		
		std::vector<char> out_buf(deflateBound(&zs, cb));
		zs.next_out = reinterpret_cast<Bytef*>(&out_buf[0]);
		zs.avail_out = out_buf.size();

		deflate(&zs, Z_FINISH);
		deflateEnd(&zs);

		std::copy(out_buf.begin(), out_buf.end(), I);
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
		zs.avail_in = cb;

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

#include "lzma/Lzma2Enc.h"
#include "lzma/Lzma2Dec.h"

namespace pack
{
	namespace detail
	{
		inline void* alloc_func(void* p, size_t cb)
		{
			return malloc(cb);
		}

		inline void free_func(void* p, void* addr)
		{
			free(addr);
		}

		struct source : ISeqInStream
		{
			source(void const* src, size_t cb)
				: src((uint8_t const*)src), cb(cb)
			{
				Read = &read;
			}

			static SRes read(void* p, void* buf, size_t* size)
			{
				auto self = (source*)p;
				auto dst = (uint8_t*)buf;

				size_t n = std::min(*size, self->cb);
				std::copy(self->src, self->src + n, dst);

				self->cb -= n;
				self->src += n;
				*size = n;

				return SZ_OK;
			}

			uint8_t const* src;
			size_t cb;
		};

		inline source make_source(void const* src, size_t cb)
		{
			return source(src, cb);
		}

		template <typename Iterator>
		struct sink : ISeqOutStream
		{
			sink(Iterator I)
				: I(I)
			{
				Write = &write;
			}

			static size_t write(void* p, void const* buf, size_t size)
			{
				auto self = (sink*)p;
				auto src = (uint8_t const*)buf;
				std::copy(src, src + size, self->I);
				return size;
			}

			Iterator I;
		};

		template <typename Iterator>
		sink<Iterator> make_sink(Iterator I)
		{
			return sink<Iterator>(I);
		}

		struct allocs
		{
			allocs()
			{
				mem_fns.Alloc = &detail::alloc_func;
				mem_fns.Free = &detail::free_func;
			}

			ISzAlloc mem_fns;
		};

		struct encoder : boost::noncopyable, allocs
		{
			encoder()
			{
				p = Lzma2Enc_Create(&mem_fns, &mem_fns);
				Lzma2EncProps_Init(&props2);
				auto& props = props2.lzmaProps;
				props.level = 6;
				props.fb = 128;
				props.algo = 1;
				props.numThreads = 1;
				Lzma2Enc_SetProps(p, &props2);
			}

			~encoder()
			{
				Lzma2Enc_Destroy(p);
			}

			CLzma2EncHandle p;
			CLzma2EncProps props2;
		};

		struct decoder : boost::noncopyable, allocs
		{
			decoder(Byte prop)
			{
				Lzma2Dec_Allocate(&dec, prop, &mem_fns);
				Lzma2Dec_Init(&dec);
			}

			~decoder()
			{
				Lzma2Dec_Free(&dec, &mem_fns);
			}

			CLzma2Dec dec;
		};

		extern boost::thread_specific_ptr<encoder> enc;
	}

	/* LZMA stream:
	 *   1 byte props
	 *   4 bytes LE unpacked size
	 *   compressed data
	 */

	template <typename Iterator>
	bool lzma_pack(void const* src, size_t cb, Iterator I)
	{
		if (!detail::enc.get())
			detail::enc.reset(new detail::encoder);

		auto& h = detail::enc->p;

		auto os = detail::make_sink(I);
		auto is = detail::make_source(src, cb);

		Byte b = Lzma2Enc_WriteProperties(h);
		os.Write(&os, &b, 1);
		uint32_t cb_i = cb;
		os.Write(&os, &cb_i, 4);
		auto res = Lzma2Enc_Encode(h, &os, &is, NULL);
		return res == SZ_OK;
	}

	template <typename Iterator>
	bool lzma_unpack(void const* src, size_t cb, Iterator I)
	{
		auto os = detail::make_sink(I);
		auto is = detail::make_source(src, cb);

		Byte b;
		size_t sz = 1;
		is.Read(&is, &b, &bn);
		if (sz != 1)
			return false;

		uint32_t cb_i;
		sz = 4;
		is.Read(&is, &cb_i, &sz);
		if (sz != 4 || sz >= (1 << 20)) // needs more clever setup for streaming
			return false;

		std::vector<uint8_t> buf(sz);

		decoder dec(b);
		ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
		SRes res = Lzma2Dec_DecodeToBuf(&dec.dec, &buf[0], buf.size(), src, cb, LZMA_FINISH_ANY, &status);
		return res == SZ_OK && status == LZMA_STATUS_FINISHED_WITH_MARK;
	}
}