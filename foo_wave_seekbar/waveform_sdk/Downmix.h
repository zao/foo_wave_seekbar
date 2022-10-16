//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../../pfc/list.h"

template<typename T>
void
get_downmix_coefficients(t_size n, pfc::list_hybrid_t<T, 18>& left, pfc::list_hybrid_t<T, 18>& right)
{
    T zero = T(0.0), one = T(1.0), sqrt_half = T(0.70710678118654752440084436210485), half = T(0.5);
    switch (n) {
        case 1: { //      { center }
            T l[] = { one };
            left = l;
            T r[] = { one };
            right = r;
            break;
        }
        case 2: { //      { left , right }
            T l[] = { one, zero };
            left = l;
            T r[] = { zero, one };
            right = r;
            break;
        }
        case 4: { //      { left , right , surr-left , surr-right }
            T l[] = { one, zero, one, zero };
            left = l;
            T r[] = { zero, one, zero, one };
            right = r;
            break;
        }
        case 5: { //      { left , right , center    , surr-left , surr-right }
            T l[] = { one, zero, sqrt_half, sqrt_half, zero };
            left = l;
            T r[] = { zero, one, sqrt_half, zero, sqrt_half };
            right = r;
            break;
        }
        case 6: { //      { left , right , center    , LFE  , surr-left ,
                  //      surr-right
                  //      }
            T l[] = { one, zero, sqrt_half, half, sqrt_half, zero };
            left = l;
            T r[] = { zero, one, sqrt_half, half, zero, sqrt_half };
            right = r;
            break;
        }
        case 8: { //      { left , right , center    , LFE  , surr-left ,
                  //      surr-right , back-left , back-right }
            T l[] = { one, zero, sqrt_half, half, sqrt_half, zero, sqrt_half, zero };
            left = l;
            T r[] = { zero, one, sqrt_half, half, zero, sqrt_half, zero, sqrt_half };
            right = r;
            break;
        }
        default: {
            pfc::list_hybrid_t<T, 18> c;
            c.set_size(n);
            for (t_size i = 0; i < n; ++i) {
                c[i] = T(1.0);
            }
            left = c;
            right = c;
        }
    }
}

template<typename T>
T
downmix_to_mono(T const* frame, size_t n_ch)
{
    static pfc::list_hybrid_t<T, 18> left, right;
    static bool once = [&]() -> bool {
        get_downmix_coefficients(n_ch, left, right);
        return false;
    }();
    T ret = T(0.0);
    for (t_size i = 0; i < n_ch; ++i) {
        ret += T(0.5) * (left[i] * frame[i] + right[i] * frame[i]);
    }
    return ret;
}

template<typename T>
std::pair<T, T>
downmix_to_stereo(T const* frame, size_t n_ch)
{
    typedef std::pair<T, T> R;
    static pfc::list_hybrid_t<T, 18> left, right;
    static bool once = [&]() -> bool {
        get_downmix_coefficients(n_ch, left, right);
        return false;
    }();
    R ret = R(T(0.0), T(0.0));
    for (t_size i = 0; i < n_ch; ++i) {
        ret.first += left[i] * frame[i];
        ret.second += right[i] * frame[i];
    }
    return ret;
}

template<typename T>
T
downmix(T const* frame, size_t n_ch)
{
    return downmix_to_mono(frame, n_ch);
}
