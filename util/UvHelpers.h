#pragma once

#include <uv.h>

template <typename Lockable>
struct lock_guard
{
	lock_guard(Lockable& m) : _m(m) {
		_m.lock();
	}

	~lock_guard() {
		_m.unlock();
	}

private:
	lock_guard(lock_guard const&) = delete;
	lock_guard& operator = (lock_guard const&) = delete;

	Lockable& _m;
};

template <>
struct lock_guard<uv_mutex_t>
{
	lock_guard(uv_mutex_t& m) : _m(m) {
		uv_mutex_lock(&_m);
	}

	~lock_guard() {
		uv_mutex_unlock(&_m);
	}

private:
	lock_guard(lock_guard const&) = delete;
	lock_guard& operator = (lock_guard const&) = delete;

	uv_mutex_t& _m;
};

template <typename T>
struct future_value
{
	uv_mutex_t mutex;
	uv_cond_t cond;

	future_value()
		: is_set(false)
	{
		uv_mutex_init(&mutex);
		uv_cond_init(&cond);
	}

	~future_value()
	{
		uv_cond_destroy(&cond);
		uv_mutex_destroy(&mutex);
	}

	enum result {
		TIMEOUT, READY
	};

	result try_get(uint64_t timeout, T& out)
	{
		lock_guard<uv_mutex_t> lk(mutex);
		while (1) {
			auto rc = uv_cond_timedwait(&cond, &mutex, 200*1000*1000ull);
			if (rc == -1) return TIMEOUT;
			if (!is_set) continue;
			out = value;
			return READY;
		}
	}

	T get()
	{
		lock_guard<uv_mutex_t> lk(mutex);
		while (!is_set) {
			uv_cond_wait(&cond, &mutex);
		}
		return value;
	}

	void set(T t)
	{
		lock_guard<uv_mutex_t> lk(mutex);
		value = t;
		is_set = true;
		uv_cond_signal(&cond);
	}

private:
	bool is_set;
	T value;

	future_value(future_value const&) = delete;
	future_value& operator = (future_value const&) = delete;
};

struct post_work
{
	uv_work_t work;
	std::function<void()> func;

	explicit post_work(std::function<void()> func) : func(func) {}

	static uv_work_t* make(std::function<void()> func) {
		return (uv_work_t*)new post_work(func);
	}
	static int queue(uv_loop_t* loop, uv_work_t* req) {
		return uv_queue_work(loop, req, do_it, done);
	}
	static void do_it(uv_work_t* req) { ((post_work*)req)->func(); }
	static void done(uv_work_t* req, int status) { delete (post_work*)req; }
};

struct async_post_queue
{
	uv_async_t async;
	uv_mutex_t mutex;
	std::list<std::function<void()>> funcs;

	explicit async_post_queue(uv_loop_t* loop) {
		uv_async_init(loop, &async, do_it);
		uv_mutex_init(&mutex);
	}

	void stop() {
		uv_close((uv_handle_t*)&async, nullptr);
	}

	~async_post_queue() {
		uv_mutex_destroy(&mutex);
		stop();
	}

	template <typename F>
	void post(F f) {
		lock_guard<uv_mutex_t> lk(mutex);
		funcs.push_back(f);
		uv_async_send(&async);
	}

	static void do_it(uv_async_t* req, int status) {
		auto* queue = (async_post_queue*)req;
		lock_guard<uv_mutex_t> lk(queue->mutex);
		while (!queue->funcs.empty()) {
			auto fun = queue->funcs.front();
			queue->funcs.pop_front();
			uv_mutex_unlock(&queue->mutex);
			fun();
			uv_mutex_lock(&queue->mutex);
		}
	}
};

struct recursive_mutex
{
	std::atomic<long> owning_id;
	std::atomic<long> lock_count;
	uv_mutex_t underlying_mutex;

	recursive_mutex() {
		owning_id = 0;
		lock_count = 0;
		uv_mutex_init(&underlying_mutex);
	}

	~recursive_mutex() {
		uv_mutex_destroy(&underlying_mutex);
		lock_count = 0;
		owning_id = 0;
	}

	void lock() {
		long self_id = GetCurrentThreadId();
		if (self_id != owning_id) {
			uv_mutex_lock(&underlying_mutex);
			owning_id = self_id;
		}
		++lock_count;
	}

	void unlock() {
		if (--lock_count == 0) {
			owning_id = 0;
			uv_mutex_unlock(&underlying_mutex);
		}
	}
};