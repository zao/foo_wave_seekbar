//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <atomic>

struct ref_base
{
    ref_base() { ref = 0; }
    virtual ~ref_base() {}
    virtual void add_ref() { ++ref; }
    virtual void release()
    {
        if (!--ref)
            delete this;
    }

  private:
    ref_base& operator=(ref_base const&);
    ref_base(ref_base const&);
    std::atomic<unsigned int> ref;
};

template<typename T>
struct ref_ptr
{
    T* p;

    ref_ptr()
      : p(nullptr)
    {}

    explicit ref_ptr(T* p, bool add_ref = true)
      : p(p)
    {
        if (p && add_ref)
            p->add_ref();
    }

    ref_ptr(ref_ptr const& rhs)
      : p(rhs.p)
    {
        if (p)
            p->add_ref();
    }

    template<typename Derived>
    ref_ptr(ref_ptr<Derived> const& rhs)
      : p(rhs.p)
    {
        if (p)
            p->add_ref();
    }

    ref_ptr& operator=(ref_ptr const& rhs)
    {
        if (this != &rhs && p != rhs.p) {
            if (p)
                p->release();
            p = rhs.p;
            if (p)
                p->add_ref();
        }
        return *this;
    }

    ~ref_ptr()
    {
        if (p)
            p->release();
    }

    void reset(T* p = nullptr, bool add_ref = true)
    {
        if (p && add_ref)
            p->add_ref();
        if (this->p)
            this->p->release();
        this->p = p;
    }

    T* operator*() const { return p; }
    T* operator->() const { return p; }

    bool is_valid() const { return !!p; }

    typedef void (ref_ptr::*operator_bool_type)();
    operator operator_bool_type() const { return (p ? &ref_ptr::operator_bool_dummy : nullptr); }

  private:
    void operator_bool_dummy() {}
};
