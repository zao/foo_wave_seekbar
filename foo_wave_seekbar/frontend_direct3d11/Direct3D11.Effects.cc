//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D11.h"
#include "Direct3D11.h"
#include "Direct3D11.Effects.h"
#include "../resource.h"
#include <sstream>
#include <string>

#include <d3dcompiler.h>

template<typename Cont, typename Pred>
typename Cont::size_type
nuke_if(Cont& c, Pred p)
{
    auto count = c.size();
    c.erase(std::remove_if(begin(c), end(c), p), end(c));
    return count - c.size();
}

namespace wave {
namespace direct3d11 {
typedef std::deque<std::string> entry_list;

effect_compiler_impl::effect_compiler_impl(CComPtr<ID3D11Device> dev, CComPtr<ID3D11DeviceContext> ctx)
  : dev(dev)
  , ctx(ctx)
{
}

bool
effect_compiler_impl::compile_fragment(ref_ptr<effect_handle>& effect,
                                       effect_compiler::diagnostic_sink const& output,
                                       char const* source,
                                       size_t source_cb)
{
    effect.reset();

    if (source_cb == 0)
        return false;

    std::vector<char> fx_body(source, source + source_cb);
    if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; })) {
        auto message =
          "Effect contained non-ASCII code units. Remove any characters with diacritics or other cool text.\n";
        output.on_error(message);
    }

    {
        CComPtr<ID3DX11Effect> fx;
        CComPtr<ID3DBlob> err;
        HRESULT hr = S_OK;
        DWORD hlsl_flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
        DWORD fx_flags = 0;
#if defined(_DEBUG)
        hlsl_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL0;
#endif
        hr = D3DX11CompileEffectFromMemory(
          fx_body.data(), fx_body.size(), "seekbar.fx", nullptr, nullptr, hlsl_flags, fx_flags, dev, &fx, &err);
        if (FAILED(hr)) {
            std::deque<diagnostic_entry> errors;
            typedef char const* iter;
            if (err) {
                iter first = (char*)err->GetBufferPointer(), last = first + err->GetBufferSize();
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

effect_impl::effect_impl(CComPtr<ID3DX11Effect> fx)
  : fx(fx)
{
}

CComPtr<ID3DX11Effect>
effect_impl::get_effect() const
{
    return fx;
}
}
}
