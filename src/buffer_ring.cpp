#include "buffer_ring.hpp"

#include <unistd.h>

#include <cstdlib>

#include "io_uring.hpp"

namespace co_uring_http
{
buffer_ring &buffer_ring::get_instance() noexcept
{
	thread_local buffer_ring instance;
	return instance;
}

// Register the buffer ring with the specified size and buffer size.
void buffer_ring::register_buffer_ring(const unsigned int buffer_ring_size, const size_t buffer_size)
{
	const size_t ring_entries_size = buffer_ring_size * sizeof(io_uring_buf);
	const size_t page_alignment = sysconf(_SC_PAGESIZE);
	void *buffer_ring = nullptr;
	posix_memalign(&buffer_ring, page_alignment, ring_entries_size);
	buffer_ring_.reset(reinterpret_cast<io_uring_buf_ring *>(buffer_ring));

	buffer_list_.reserve(buffer_ring_size);
	for (unsigned int i = 0; i < buffer_ring_size; ++i)
	{
		buffer_list_.emplace_back(buffer_size);
	}

	io_uring::get_instance().setup_buffer_ring(buffer_ring_.get(), buffer_list_, buffer_list_.size());
}

// Borrow a buffer from the buffer ring.
std::span<char> buffer_ring::borrow_buffer(const unsigned int buffer_id, const size_t size)
{
	borrowed_buffer_set_[buffer_id] = true;
	return {buffer_list_[buffer_id].data(), size};
}

// Return the buffer to the buffer ring.
void buffer_ring::return_buffer(const unsigned int buffer_id)
{
	borrowed_buffer_set_[buffer_id] = false;
	io_uring::get_instance().add_buffer(
		buffer_ring_.get(), buffer_list_[buffer_id], buffer_id, buffer_list_.size());
}
} // namespace co_uring_http
