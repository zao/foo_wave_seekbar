//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <zlib.h>
#include "lzma/Lzma2Enc.h"
#include "lzma/Lzma2Dec.h"
#include <zstd.h>

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

namespace pack {
template<typename OutputIterator>
bool
zstd_pack(std::span<std::byte const> src, OutputIterator I)
{
    ZSTD_inBuffer in_buf{ .src = src.data(), .size = src.size_bytes(), .pos{} };
    std::vector<uint8_t> dst(ZSTD_CStreamOutSize());
    ZSTD_outBuffer out_buf{ .dst = dst.data(), .size = dst.size(), .pos{} };

    std::shared_ptr<ZSTD_CStream> cctx(ZSTD_createCStream(), &ZSTD_freeCStream);
    ZSTD_initCStream(cctx.get(), 5);

    while (true) {
        auto n =
          ZSTD_compressStream2(cctx.get(), &out_buf, &in_buf, ZSTD_e_end);
        if (ZSTD_isError(n)) {
            return false;
        }
        std::copy_n(dst.begin(), out_buf.pos, I);
        if (n > 0) {
            out_buf.pos = 0;
        } else {
            return true;
        }
    }
}

template<typename OutputIterator>
bool
zstd_unpack(std::span<std::byte const> src, OutputIterator I)
{
    ZSTD_inBuffer in_buf{ .src = src.data(), .size = src.size_bytes(), .pos{} };
    std::vector<uint8_t> dst(ZSTD_DStreamOutSize());
    ZSTD_outBuffer out_buf{ .dst = dst.data(), .size = dst.size(), .pos{} };

    std::shared_ptr<ZSTD_DStream> dctx(ZSTD_createDStream(), &ZSTD_freeDStream);
    ZSTD_initDStream(dctx.get());

    while (true) {
        auto n = ZSTD_decompressStream(dctx.get(), &out_buf, &in_buf);
        if (ZSTD_isError(n)) {
            return false;
        }
        std::copy_n(dst.data(), out_buf.pos, I);
        if (n > 0) {
            out_buf.pos = 0;
        } else {
            return true;
        }
    }
}
}

namespace pack {
template<typename Iterator>
bool
z_pack(std::span<std::byte const> src, Iterator I)
{
    z_stream zs = {};
    zs.zalloc = nullptr;
    zs.zfree = nullptr;
    zs.opaque = nullptr;

    deflateInit(&zs, Z_DEFAULT_COMPRESSION);

    zs.next_in = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(src.data()));
    zs.avail_in = src.size_bytes();

    std::vector<char> out_buf(deflateBound(&zs, src.size_bytes()));
    zs.next_out = reinterpret_cast<Bytef*>(&out_buf[0]);
    zs.avail_out = out_buf.size();

    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    std::copy_n(out_buf.begin(), zs.total_out, I);
    return true;
}

template<typename Iterator>
bool
z_unpack(std::span<std::byte const> src, Iterator I)
{
    z_stream zs = {};
    zs.zalloc = nullptr;
    zs.zfree = nullptr;
    zs.opaque = nullptr;

    zs.next_in = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(src.data()));
    zs.avail_in = static_cast<uInt>(src.size_bytes());

    inflateInit(&zs);

    size_t const OUT_CB = 1024;
    char buf[OUT_CB];
    zs.next_out = reinterpret_cast<Bytef*>(buf);
    zs.avail_out = OUT_CB;

    while (1) {
        int res = inflate(&zs, Z_NO_FLUSH);
        if (res < 0)
            return false;
        if (zs.avail_out != OUT_CB) {
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

namespace pack {
namespace detail {
inline void*
alloc_func(void* p, size_t cb)
{
    return malloc(cb);
}

inline void
free_func(void* p, void* addr)
{
    free(addr);
}

struct source : ISeqInStream
{
    source(void const* src, size_t cb)
      : src(static_cast<uint8_t const*>(src))
      , cb(cb)
    {
        Read = &read;
    }

    static SRes read(void* p, void* buf, size_t* size)
    {
        auto self = static_cast<source*>(p);
        auto dst = static_cast<uint8_t*>(buf);

        size_t n = (std::min<size_t>)(2048u, (std::min)(*size, self->cb));
        std::memcpy(dst, self->src, n);

        self->cb -= n;
        self->src += n;
        *size = n;

        return SZ_OK;
    }

    uint8_t const* src;
    size_t cb;
};

inline source
make_source(void const* src, size_t cb)
{
    return source(src, cb);
}

template<typename Iterator>
struct sink : ISeqOutStream
{
    sink(Iterator I)
      : I(I)
    {
        Write = &write;
    }

    static size_t write(void* p, void const* buf, size_t size)
    {
        auto self = static_cast<sink*>(p);
        auto src = static_cast<uint8_t const*>(buf);
        std::copy(src, src + size, self->I);
        return size;
    }

    Iterator I;
};

template<typename Iterator>
sink<Iterator>
make_sink(Iterator I)
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

struct encoder : allocs
{
    encoder()
    {
        p = Lzma2Enc_Create(&mem_fns, &mem_fns);
        Lzma2EncProps_Init(&props2);
        auto& props = props2.lzmaProps;
        props.level = 1;
        props.fb = 128;
        props.algo = 1;
        props.numThreads = 1;
        props.writeEndMark = 1;
        Lzma2Enc_SetProps(p, &props2);
    }

    ~encoder() { Lzma2Enc_Destroy(p); }

    CLzma2EncHandle p;
    CLzma2EncProps props2;

  private:
    encoder(encoder const&);
    encoder& operator=(encoder const&);
};

struct decoder : allocs
{
    decoder(Byte prop)
    {
        CLzma2Dec _ = {};
        dec = _;
        Lzma2Dec_Construct(&dec);
        Lzma2Dec_Allocate(&dec, prop, &mem_fns);
        Lzma2Dec_Init(&dec);
    }

    ~decoder() { Lzma2Dec_Free(&dec, &mem_fns); }

    CLzma2Dec dec;

  private:
    decoder(decoder const&);
    decoder& operator=(decoder const&);
};
}

/* LZMA stream:
 *   1 byte props
 *   4 bytes LE unpacked size
 *   compressed data
 */

template<typename Iterator>
bool
lzma_pack(std::span<std::byte const> src, Iterator I)
{
    detail::encoder enc;

    auto& h = enc.p;

    auto os = detail::make_sink(I);
    auto is = detail::make_source(src.data(), src.size_bytes());

    Byte b = Lzma2Enc_WriteProperties(h);
    os.Write(&os, &b, 1);
    uint32_t cb_i = static_cast<uint32_t>(src.size_bytes());
    os.Write(&os, &cb_i, 4);
    auto res = Lzma2Enc_Encode(h, &os, &is, nullptr);
    return res == SZ_OK;
}

template<typename Iterator>
bool
lzma_unpack(std::span<std::byte const> src, Iterator I)
{
    size_t cb = src.size_bytes();
    auto os = detail::make_sink(I);
    auto is = detail::make_source(src.data(), cb);

    Byte b;
    size_t sz = 1;
    is.Read(&is, &b, &sz);
    if (sz != 1)
        return false;

    size_t dst_len;
    {
        uint32_t cb_i;
        sz = 4;
        is.Read(&is, &cb_i, &sz);
        if (sz != 4 ||
            cb_i >= (1 << 20)) // needs more clever setup for streaming
            return false;
        dst_len = cb_i;
    }

    std::vector<uint8_t> buf(dst_len);

    detail::decoder dec(b);
    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    cb -= 5;
    SRes res = Lzma2Dec_DecodeToBuf(&dec.dec,
                                    &buf[0],
                                    &dst_len,
                                    (Byte const*)src.data() + 5,
                                    &cb,
                                    LZMA_FINISH_END,
                                    &status);
    if (res == SZ_OK && status == LZMA_STATUS_FINISHED_WITH_MARK) {
        os.Write(&os, &buf[0], dst_len);
        return true;
    }
    return false;
}
}
