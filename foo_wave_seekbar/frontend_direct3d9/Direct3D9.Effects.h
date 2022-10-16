//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "Direct3D9.h"

namespace wave {
namespace direct3d9 {
struct effect_compiler_impl : effect_compiler
{
    explicit effect_compiler_impl(CComPtr<ID3D11Device> dev);
    virtual bool compile_fragment(ref_ptr<effect_handle>& effect,
                                  diagnostic_sink const& output,
                                  std::span<char const> data,
                                  std::span<D3D11_INPUT_ELEMENT_DESC const> input_descs);

  private:
    CComPtr<ID3D11Device> dev;
};

struct effect_impl : effect_handle
{
    effect_impl(CComPtr<ID3DX11Effect> fx);

    CComPtr<ID3DX11Effect> get_effect() const override;
    CComPtr<ID3D11InputLayout> get_input_layout_for_pass(size_t pass_idx) const override;
    void set_input_layouts(std::span<CComPtr<ID3D11InputLayout> const> input_layouts) override;

  private:
    CComPtr<ID3DX11Effect> fx;
    std::vector<CComPtr<ID3D11InputLayout>> pass_layouts;
};
}
}
