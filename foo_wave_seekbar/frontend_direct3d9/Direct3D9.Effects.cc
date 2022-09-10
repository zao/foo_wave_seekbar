//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
#include "resource.h"
#include <sstream>
#include <string>

template<typename Cont, typename Pred>
typename Cont::size_type
nuke_if(Cont& c, Pred p)
{
    auto count = c.size();
    c.erase(std::remove_if(begin(c), end(c), p), end(c));
    return count - c.size();
}

namespace wave {
namespace direct3d9 {
typedef std::deque<std::string> entry_list;

effect_compiler_impl::effect_compiler_impl(d3d9_api const& api,
                                           CComPtr<IDirect3DDevice9> dev)
  : api(api)
  , dev(dev)
{}

bool
effect_compiler_impl::compile_fragment(
  ref_ptr<effect_handle>& effect,
  effect_compiler::diagnostic_sink const& output,
  char const* source,
  size_t source_cb)
{
    effect.reset();

    if (source_cb == 0)
        return false;

    std::vector<char> fx_body(source, source + source_cb);
    if (size_t diff =
          nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; })) {
        auto message = "Effect contained non-ASCII code units. Remove any "
                       "characters with diacritics or other moonspeak.\n";
        output.on_error(message);
    }

    {
        CComPtr<ID3DXEffect> fx;
        CComPtr<ID3DXBuffer> err;
        HRESULT hr = S_OK;
        DWORD flags = 0;
#if defined(_DEBUG)
        flags |= D3DXSHADER_DEBUG | D3DXSHADER_OPTIMIZATION_LEVEL0;
#endif
        hr = api.D3DXCreateEffect(dev,
                                  &fx_body[0],
                                  fx_body.size(),
                                  nullptr,
                                  nullptr,
                                  flags,
                                  nullptr,
                                  &fx,
                                  &err);
        if (FAILED(hr)) {
            std::deque<diagnostic_entry> errors;
            typedef char const* iter;
            if (err) {
                iter first = (char*)err->GetBufferPointer(),
                     last = first + err->GetBufferSize();
                std::istringstream iss(std::string(first, last));
                std::string line;
                while (std::getline(iss, line)) {
                    output.on_error(line.c_str());
                }
            }
            return false;
        }
        effect.reset(new effect_impl(fx));
    }
    return true;
}

effect_impl::effect_impl(CComPtr<ID3DXEffect> fx)
  : fx(fx)
{}

CComPtr<ID3DXEffect>
effect_impl::get_effect() const
{
    return fx;
}
}
}
