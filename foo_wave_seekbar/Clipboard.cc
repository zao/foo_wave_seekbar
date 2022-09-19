//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Clipboard.h"
#include <SDK/input.h>

#include <mmreg.h>
#include <deque>
#include <span>

// TODO(zao): Audit this file, may be completely broken

namespace clipboard {
template<class T>
struct chunk_traits;

struct fmt_chunk
{
    WAVEFORMATEXTENSIBLE wfex;
};

struct data_chunk
{
    data_chunk()
      : cb(0)
    {
    }
    size_t cb;
};

struct riff_chunk
{
    riff_chunk() { memcpy(format, "WAVE", 4); }
    char format[4];
    fmt_chunk fmt;
    data_chunk data;
};

template<>
struct chunk_traits<fmt_chunk>
{
    static char const* tag() { return "fmt "; }
    static uint32_t size(fmt_chunk const& t) { return self_size(t); }

    static uint32_t self_size(fmt_chunk const&) { return 0; }
};

template<>
struct chunk_traits<data_chunk>
{
    static char const* tag() { return "data"; }
    static uint32_t size(data_chunk const& t)
    {
        return self_size(t) + static_cast<uint32_t>(t.cb);
    }

    static uint32_t self_size(data_chunk const&) { return 0; }
};

template<>
struct chunk_traits<riff_chunk>
{
    static char const* tag() { return "RIFF"; }
    static uint32_t size(riff_chunk const& t)
    {
        return self_size(t) + 8 + chunk_traits<fmt_chunk>::size(t.fmt) + 8 +
               chunk_traits<data_chunk>::size(t.data);
    }

    static uint32_t self_size(riff_chunk const&) { return 4; }
};

struct storage
{
    HGLOBAL mem;
    t_int64 data_offset;
    t_int64 frames_written;
    t_int64 frame_count;
    riff_chunk framing;

    storage()
      : mem(nullptr)
      , frames_written(0)
    {
    }

    ~storage()
    {
        if (mem)
            GlobalFree(mem);
    }

    bool append(audio_chunk const& chunk)
    {
        if (is_full())
            return false;
        const auto nch = framing.fmt.wfex.Format.nChannels;

        char* dst = static_cast<char*>(GlobalLock(mem)) + data_offset +
                    frames_written * nch * sizeof(float);
        audio_sample const* src = chunk.get_data();
        const size_t n =
          (std::min)(chunk.get_sample_count(),
                     static_cast<size_t>(frame_count - frames_written));
        for (const auto sample : std::span(src, n * nch)) {
            auto val = static_cast<float>(sample);
            std::memcpy(dst, &val, sizeof(float));
            dst += sizeof(float);
        }
        GlobalUnlock(mem);
        frames_written += n;
        return !is_full();
    }

    bool is_full() const { return frames_written == frame_count; }

    HGLOBAL transfer()
    {
        HGLOBAL ret = nullptr;
        std::swap(ret, mem);
        return ret;
    }
};

template<typename Chunk>
void*
write_chunk(Chunk const& chunk, void* dst)
{
    typedef chunk_traits<Chunk> traits;
    char* buf = static_cast<char*>(dst);
    auto chunk_id = traits::tag();
    const uint32_t chunk_size = traits::size(chunk);
    const uint32_t self_size = traits::self_size(chunk);
    std::memcpy(buf, chunk_id, 4);
    buf += 4;
    std::memcpy(buf, &chunk_size, 4);
    buf += 4;
    std::memcpy(buf, &chunk, self_size);
    buf += 4;
    return buf;
}

std::shared_ptr<storage>
make_storage(unsigned channel_count,
             unsigned channel_config,
             unsigned sample_rate,
             double sample_length)
{
    auto ret = std::make_shared<storage>();
    ret->frame_count = static_cast<t_int64>(sample_rate * sample_length);
    ret->data_offset = 8 + chunk_traits<riff_chunk>::size(ret->framing);

    ret->framing.data.cb =
      static_cast<size_t>(ret->frame_count * 4 * channel_count);
    const size_t total_bytes = chunk_traits<riff_chunk>::size(ret->framing);
    WAVEFORMATEXTENSIBLE& wf = ret->framing.fmt.wfex;
    wf.Samples.wValidBitsPerSample = 32;
    wf.dwChannelMask = channel_config;
    wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    const auto block_align = channel_count * 4;

    WAVEFORMATEX& f = wf.Format;
    f.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.nChannels = channel_count;
    f.nSamplesPerSec = sample_rate;
    f.nAvgBytesPerSec = sample_rate * block_align;
    f.nBlockAlign = block_align;
    f.wBitsPerSample = 32;
    f.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    ret->mem = GlobalAlloc(GMEM_MOVEABLE, total_bytes);
    void* p = GlobalLock(ret->mem);
    p = write_chunk(ret->framing, p);
    p = write_chunk(ret->framing.fmt, p);
    p = write_chunk(ret->framing.data, p);
    GlobalUnlock(ret->mem);

    return ret;
}

static double constexpr MaxCopyDuration = 600.0;
bool
render_audio(metadb_handle_ptr source, double beginning, double end)
{
    if (end - beginning > MaxCopyDuration) {
        console::warning(
          "Seekbar: Cowardly refusing to copy more than 10 minutes.");
        return false;
    }

    service_ptr_t<input_decoder> decoder;
    abort_callback_impl abort_cb;

    input_entry::g_open_for_decoding(
      decoder, nullptr, source->get_path(), abort_cb);

    {
        decoder->initialize(
          source->get_subsong_index(), input_flag_simpledecode, abort_cb);
        if (!decoder->can_seek())
            return false;

        decoder->seek(beginning, abort_cb);

        std::deque<audio_chunk_impl> chunks;
        t_int64 samples_decoded = 0;

        std::shared_ptr<storage> clip_storage;

        while (true) {
            audio_chunk_impl chunk;
            if (!decoder->run(chunk, abort_cb)) {
                return false;
            }

            if (!clip_storage) {
                const auto channel_count = chunk.get_channels();
                const auto channel_config = chunk.get_channel_config();
                const auto sample_rate = chunk.get_sample_rate();
                clip_storage = make_storage(
                  channel_count, channel_config, sample_rate, end - beginning);
            }
            if (!clip_storage->append(chunk))
                break;
        }

        if (!clip_storage || !clip_storage->is_full()) {
            return false;
        }

        const HGLOBAL mem = clip_storage->transfer();
        bool opened = !!OpenClipboard(nullptr);
        bool emptied = !!EmptyClipboard();
        const bool success = !!SetClipboardData(CF_WAVE, mem);
        if (!success)
            GlobalFree(mem);
        bool closed = !!CloseClipboard();
        return true;
    }
}
}
