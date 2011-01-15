#pragma once

namespace wave
{
	struct active_data_base
	{
		virtual ~active_data_base() {}
	};

	template <typename> struct active_data;

	template <typename T>
	struct active_data_listener
	{
		virtual ~active_data_listener() {}
		virtual void on_write(active_data<T> const& value) {};
	};

	template <typename T>
	struct active_data_function_listener : active_data_listener<T>
	{
		typedef void (write_type)(active_data<T> const&);
		virtual void on_write(active_data<T> const& value) { if (write) write(value); }
		boost::function<write_type> write;
	};

	template <typename T>
	shared_ptr<active_data_function_listener<T>> make_active_data_function_listener(boost::function<typename active_data_function_listener<T>::write_type> write)
	{
		auto ret = make_shared<active_data_function_listener<T>>();
		ret->write = write;
		return ret;
	}
	
	template <typename T>
	struct active_data
	{
		explicit active_data(T t = T()) : value(new T(t)), listeners(new listener_collection()) {}
		T& set(T const& rhs) { *value = rhs; on_write(); return *value; }
		T const& get() const { return *value; }

		void subscribe(weak_ptr<active_data_listener<T>> ptr)
		{
			listeners->insert(ptr);
		}

	private:
		void on_write()
		{
			for (auto I = listeners->begin(); I != listeners->end(); )
			{	
				if (auto ptr = I->lock())
				{
					ptr->on_write(*this);
					++I;
				}
				else
				{
					I = listeners->erase(I);
				}
			}
		}

		typedef std::set<weak_ptr<active_data_listener<T>>> listener_collection;
		boost::shared_ptr<T> value;
		boost::shared_ptr<listener_collection> listeners;
	};	

	struct get_impl
	{
		template <typename T>
		struct result { typedef T const& type; };

		template <typename T>
		T const& operator () (active_data<T> const& ad) const { return ad.get(); }
	};

	static boost::phoenix::function<get_impl> get;

	struct set_impl
	{
		template <typename T>
		struct result { typedef T& type; };

		template <typename T>
		T& operator () (active_data<T>& ad, T const& t) const { return ad.set(t); }
	};

	static boost::phoenix::function<set_impl> set;
};