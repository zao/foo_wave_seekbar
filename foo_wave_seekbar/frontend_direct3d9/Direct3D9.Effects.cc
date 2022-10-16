//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
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
namespace direct3d9 {
typedef std::deque<std::string> entry_list;

effect_compiler_impl::effect_compiler_impl(CComPtr<ID3D11Device> dev)
  : dev(dev)
{
}

bool
effect_compiler_impl::compile_fragment(ref_ptr<effect_handle>& effect,
                                       effect_compiler::diagnostic_sink const& output,
                                       std::span<char const> source,
                                       std::span<D3D11_INPUT_ELEMENT_DESC const> input_descs)
{
    effect.reset();

    if (source.empty())
        return false;

    std::vector<char> fx_body(source.begin(), source.end());
    if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; })) {
        auto message = "Effect contained non-ASCII code units. Remove any "
                       "characters with diacritics or other moonspeak.\n";
        output.on_error(message);
    }

    {
        CComPtr<ID3DX11Effect> fx;
        CComPtr<ID3DBlob> err;
        HRESULT hr = S_OK;
        DWORD flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL0;
#endif
        DWORD fx_flags = 0;
        hr = D3DX11CompileEffectFromMemory(
          &fx_body[0], fx_body.size(), nullptr, nullptr, nullptr, flags, fx_flags, dev, &fx, &err);
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
        auto* tech = fx->GetTechniqueByIndex(0);
        D3DX11_TECHNIQUE_DESC tech_desc{};
        tech->GetDesc(&tech_desc);

        std::vector<CComPtr<ID3D11InputLayout>> layouts;
        for (size_t pass_idx = 0; pass_idx < tech_desc.Passes; ++pass_idx) {
            auto* pass = tech->GetPassByIndex(0);
            D3DX11_PASS_SHADER_DESC vs_desc{};
            hr = pass->GetVertexShaderDesc(&vs_desc);

            D3DX11_EFFECT_SHADER_DESC vs_desc2{};
            hr = vs_desc.pShaderVariable->GetShaderDesc(vs_desc.ShaderIndex, &vs_desc2);

            CComPtr<ID3D11InputLayout> il;
            hr = dev->CreateInputLayout(
              input_descs.data(), (UINT)input_descs.size(), vs_desc2.pBytecode, vs_desc2.BytecodeLength, &il);
            if (FAILED(hr)) {
                return false;
            }
            layouts.push_back(il);
        }
        effect.reset(new effect_impl(fx));
        effect->set_input_layouts(layouts);
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

void
effect_impl::set_input_layouts(std::span<CComPtr<ID3D11InputLayout> const> input_layouts)
{
    pass_layouts.assign(input_layouts.begin(), input_layouts.end());
}

CComPtr<ID3D11InputLayout>
effect_impl::get_input_layout_for_pass(size_t pass_idx) const
{
    return pass_layouts.at(pass_idx);
}
}
}
