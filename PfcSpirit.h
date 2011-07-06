//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../../pfc/list.h"
#include <boost/spirit/home/support.hpp>
#include <boost/spirit/home/support/container.hpp>

namespace boost { namespace spirit { namespace traits
{
	template <typename T>
	struct is_container<typename pfc::list_t<T>>
		: mpl::bool_<true>
	{};

	template <typename T>
	struct container_value<typename pfc::list_t<T>>
		: detail::remove_value_const<T>
	{};

	namespace detail
	{
		template <typename T>
		struct pfc_list_const_iterator
		{
			boost::reference_wrapper<pfc::list_t<T> const> container;
			t_size offset;
		};

		template <typename T>
		struct pfc_list_iterator
		{
			boost::reference_wrapper<pfc::list_t<T>> container;
			t_size offset;
		};
	}

	template <typename T>
	struct container_iterator<typename pfc::list_t<T> const>
	{
		typedef detail::pfc_list_iterator<typename container_value<T>::type> type;
	};

	template <typename T>
	struct container_iterator<typename pfc::list_t<T>>
	{
		typedef detail::pfc_list_iterator<typename container_value<T>::type> type;
	};

	template <typename T>
	bool push_back(typename pfc::list_t<T>& c, T const& val)
	{
		c.add_item(val);
		return true;
	}

	template <typename T>
	bool is_empty(typename pfc::list_t<T> const& c)
	{
		return !c.get_count();
	}
}}}
