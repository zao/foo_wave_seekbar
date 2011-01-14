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
		virtual void on_write(active_data<T> const& value, std::string const& source) {};
	};

	template <typename T>
	struct active_data_function_listener : active_data_listener<T>
	{
		typedef void (write_type)(active_data<T> const&, std::string const&);
		virtual void on_write(active_data<T> const& value, std::string const& source) { if (write) write(value, source); }
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
		T& operator = (T const& rhs) { return set(rhs, ""); }
		T& set(T const& rhs, std::string const& who) { *value = rhs; on_write(who); return *value; }
		operator T const & () const { return *value; }

		void subscribe(weak_ptr<active_data_listener<T>> ptr)
		{
			listeners->insert(ptr);
		}

	private:
		void on_write(std::string const who)
		{
			for (auto I = listeners->begin(); I != listeners->end(); )
			{	
				if (auto ptr = I->lock())
				{
					ptr->on_write(*this, who);
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

		active_data<T>& operator = (active_data<T> const&);
	};
};