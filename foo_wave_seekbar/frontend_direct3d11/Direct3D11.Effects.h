//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "Direct3D11.h"

namespace wave {
namespace direct3d11 {
struct effect_compiler_impl : effect_compiler
{
    explicit effect_compiler_impl(CComPtr<ID3D11Device> dev, CComPtr<ID3D11DeviceContext> ctx);
    virtual bool compile_fragment(ref_ptr<effect_handle>& effect,
                                  diagnostic_sink const& output,
                                  char const* data,
                                  size_t data_bytes);

  private:
    CComPtr<ID3D11Device> dev;
    CComPtr<ID3D11DeviceContext> ctx;
};

struct effect_impl : effect_handle
{
    explicit effect_impl(CComPtr<ID3DX11Effect> fx);

    CComPtr<ID3DX11Effect> get_effect() const;

  private:
    CComPtr<ID3DX11Effect> fx;
};
}
}
