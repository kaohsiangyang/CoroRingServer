#ifndef SYNC_WAIT_HPP
#define SYNC_WAIT_HPP

#include <algorithm>
#include <atomic>
#include <optional>
#include <vector>

#include "task.hpp"

namespace co_uring_http {
template <typename T>
class sync_wait_task_promise;

template <typename T>
class sync_wait_task
{
public:
	using promise_type = sync_wait_task_promise<T>;

	explicit sync_wait_task(std::coroutine_handle<sync_wait_task_promise<T>> coroutine_handle) noexcept
		: coroutine_(coroutine_handle) {}

	~sync_wait_task()
	{
		if (coroutine_)
		{
			coroutine_.destroy();
		}
	}

	T get_return_value() const noexcept
	{
		return coroutine_.promise().get_return_value();
	}

	void wait() const noexcept
	{
		coroutine_.promise().get_atomic_flag().wait(false, std::memory_order_acquire);
	}

private:
	std::coroutine_handle<sync_wait_task_promise<T>> coroutine_;
};

template <typename T>
class sync_wait_task_promise_base
{
public:
	std::suspend_never initial_suspend() const noexcept { return {}; }

	class final_awaiter
	{
	public:
		bool await_ready() const noexcept { return false; }

		void await_resume() const noexcept {}

		void await_suspend(std::coroutine_handle<sync_wait_task_promise<T>> coroutine) const noexcept
		{
			std::atomic_flag &atomic_flag = coroutine.promise().get_atomic_flag();
			atomic_flag.test_and_set(std::memory_order_release);
			atomic_flag.notify_all();
		}
	};

	final_awaiter final_suspend() const noexcept { return {}; }

	void unhandled_exception() const noexcept { std::terminate(); }

	std::atomic_flag &get_atomic_flag() noexcept { return atomic_flag_; }

private:
	std::atomic_flag atomic_flag_;
};

template <typename T>
class sync_wait_task_promise final : public sync_wait_task_promise_base<T>
{
public:
	sync_wait_task<T> get_return_object() noexcept
	{
		return sync_wait_task<T>{std::coroutine_handle<sync_wait_task_promise<T>>::from_promise(*this)};
	}

	template <typename U>
		requires std::convertible_to<U &&, T>
	void return_value(U &&return_value) noexcept(std::is_nothrow_constructible_v<T, U &&>)
	{
		return_value_.emplace(std::forward<U>(return_value));
	}

	T get_return_value() & noexcept { return *return_value_; }

	T &&get_return_value() && noexcept { return std::move(*return_value_); }

private:
	std::optional<T> return_value_;
};

template <>
class sync_wait_task_promise<void> final : public sync_wait_task_promise_base<void>
{
public:
	sync_wait_task<void> get_return_object() noexcept
	{
		return sync_wait_task<void>{std::coroutine_handle<sync_wait_task_promise>::from_promise(*this)};
	}

	void return_void() noexcept {}
};

template <typename T>
T sync_wait(task<T> &task)
{
	auto sync_wait_task_handle = ([&]() -> sync_wait_task<T>
								  { co_return co_await task; })();
	sync_wait_task_handle.wait();

	if constexpr (!std::is_same_v<T, void>)
	{
		return sync_wait_task_handle.get_return_value();
	}
}

template <typename T>
T sync_wait(task<T> &&task)
{
	auto sync_wait_task_handle = ([&]() -> sync_wait_task<T>
								  { co_return co_await task; })();
	sync_wait_task_handle.wait();

	if constexpr (!std::is_same_v<T, void>)
	{
		return sync_wait_task_handle.get_return_value();
	}
}

template <typename T>
std::conditional_t<std::is_same_v<T, void>, void, std::vector<T>> sync_wait_all(std::vector<task<T>> &task_list)
{
	if constexpr (std::is_same_v<T, void>)
	{
		for (auto &task : task_list)
		{
			sync_wait(task);
		}
	}
	else
	{
		std::vector<T> return_value_list;
		return_value_list.reserve(task_list.size());

		std::transform(
			task_list.begin(), task_list.end(), std::back_inserter(return_value_list),
			[](task<T> &task) -> T
			{ return sync_wait(task); });
		return return_value_list;
	}
}
} // namespace co_uring_http

#endif
