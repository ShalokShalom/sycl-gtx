#pragma once

// 3.6.1 Buffers

#include "access.h"
#include "error_handler.h"
#include "event.h"
#include "ranges.h"
#include "refc.h"
#include "queue.h"
#include "../common.h"
#include "../debug.h"
#include <vector>

namespace cl {
namespace sycl {

// Forward declarations
template <typename DataType, int dimensions = 1>
struct buffer;
template <typename DataType, int dimensions, access::mode mode, access::target target>
class accessor;
class command_group;

namespace detail {

template <typename DataType, int dimensions>
class buffer_ {
private:
	range<dimensions> rang;
	refc::ptr<cl_mem> data;
	bool is_blocking = true;
	bool is_initialized = false;
	bool is_read_only = false;
	detail::error::handler handler;

	// Associated host memory.
	buffer_(const DataType* host_data, range<dimensions> range, bool is_read_only, bool is_blocking = true)
		: rang(range), is_read_only(is_read_only), is_blocking(is_blocking) {
		DSELF() << "not implemented";
	}

public:
	// Associated host memory.
	// The buffer will use this host memory for its full lifetime,
	// but the contents of this host memory are undefined for the lifetime of the buffer.
	// If the host memory is modified by the host, or mapped to another buffer or image during the lifetime of this buffer,
	// then the results are undefined.
	// The initial contents of the buffer will be the contents of the host memory at the time of construction.
	// When the buffer is destroyed, the destructor will block until all work in queues on the buffer has completed,
	// then copy the contents of the buffer back to the host memory (if required) and then return.
	buffer_(DataType* host_data, range<dimensions> range)
		: buffer_(host_data, range, false) {}

	// Associated host memory, read-only mode.
	buffer_(const DataType* host_data, range<dimensions> range)
		: buffer_(host_data, range, true) {}

	// No associated storage.
	// The storage for this type of buffer is entirely handled by the SYCL system.
	// The destructor for this type of buffer never blocks, even if work on the buffer has not completed.
	// Instead, the SYCL system frees any storage required for the buffer asynchronously when it is no longer in use in queues.
	// The initial contents of the buffer are undefined.
	buffer_(range<dimensions> range)
		: buffer_(nullptr, range, false, false) {
		DSELF() << "not implemented";
	}

	// Associated storage object.
	// The storage object must not be destroyed by the user until after the buffer has been destroyed.
	// The synchronization and copying behavior of the storage is determined by the storage object.
	//buffer(storage<DataType>& store, range<dimensions>);

	// Creates a sub-buffer object, which is a sub-range reference to a base buffer.
	// This sub-buffer can be used to create accessors to the base buffer,
	// but which only have access to the range specified at time of construction of the sub-buffer.
	//buffer_(buffer_, index<dimensions> base_index, range<dimensions> sub_range);

	// Creates a buffer from an existing OpenCL memory object associated to a context
	// after waiting for an event signaling the availability of the OpenCL data.
	// mem_object is the OpenCL memory object to use.
	// from_queue is the queue associated to the memory object.
	// available_event specifies the event to wait for if non null
	buffer_(cl_mem mem_object, queue from_queue, event available_event);

	// Return a range object representing the size of the buffer
	// in terms of number of elements in each dimension as passed to the constructor.
	range<dimensions> get_range() {
		return rang;
	}

	// Total number of elements in the buffer
	size_t get_count() {
		size_t count = 0;
		for(int i = 0; i < dimensions; ++i) {
			count += rang[i];
		}
		return count;
	}

	// Total number of bytes in the buffer
	size_t get_size() {
		return get_count() * sizeof(DataType);
	}

public:
	// TODO: Take read_only into consideration
	template<access::mode mode, access::target target = access::global_buffer>
	accessor<DataType, dimensions, mode, target> get_access() {
		if(command_group_::last == nullptr) {
			handler.report(error::code::NOT_IN_COMMAND_GROUP_SCOPE);
		}
		return accessor<DataType, dimensions, mode, target>(*reinterpret_cast<cl::sycl::buffer<DataType, dimensions>*>(this));
	}
};

} // namespace detail

// Defines a shared array that can be used by kernels in queues and has to be accessed using accessor classes.

template <typename DataType>
struct buffer<DataType, 1> : public detail::buffer_<DataType, 1> {
#if MSVC_LOW
	buffer(range<1> range)
		: detail::buffer_<DataType, 1>(range) {}
	buffer(DataType* host_data, range<1> range)
		: detail::buffer_<DataType, 1>(host_data, range) {}
	buffer(const DataType* host_data, range<1> range)
		: detail::buffer_<DataType, 1>(host_data, range) {}
	//buffer(storage<DataType>& store, range<1>);
	//buffer(buffer, index<dimensions> base_index, range<dimensions> sub_range);
	buffer(cl_mem mem_object, queue from_queue, event available_event)
		: detail::buffer_<DataType, 1>(mem_object, from_queue, available_event) {}
#else
	using detail::buffer_<DataType, 1>::buffer;
#endif
	// Create a new allocated 1D buffer initialized from the given elements
	// ranging from first up to one before last
	template <class InputIterator>
	buffer(InputIterator* startIterator, InputIterator* endIterator);

	// TODO: Used by the Codeplay SYCL example
	buffer(vector_class<DataType> host_data)
		: detail::buffer_<DataType, 1>(host_data.data(), host_data.size()) {
		DSELF() << "not implemented";
	}
};

template <typename DataType>
struct buffer<DataType, 2> : public detail::buffer_<DataType, 2>{
#if MSVC_LOW
	buffer(range<2> range)
		: detail::buffer_<DataType, 2>(range) {}
	buffer(DataType* host_data, range<2> range)
		: detail::buffer_<DataType, 2>(host_data, range) {}
	buffer(const DataType* host_data, range<2> range)
		: detail::buffer_<DataType, 2>(host_data, range) {}
	//buffer(storage<DataType>& store, range<2>);
	//buffer(buffer, index<2> base_index, range<2> sub_range);
	buffer(cl_mem mem_object, queue from_queue, event available_event)
		: detail::buffer_<DataType, 2>(mem_object, from_queue, available_event) {}
#else
	using detail::buffer_<DataType, 2>::buffer;
#endif
	buffer(size_t sizeX, size_t sizeY)
		: buffer(range<2>{ sizeX, sizeY }) {}
	buffer(DataType* host_data, size_t sizeX, size_t sizeY)
		: buffer(host_data, { sizeX, sizeY }) {}
};

template <typename DataType>
struct buffer<DataType, 3> : public detail::buffer_<DataType, 3>{
#if MSVC_LOW
	buffer(range<3> range)
		: detail::buffer_<DataType, 3>(range) {}
	buffer(DataType* host_data, range<3> range)
		: detail::buffer_<DataType, 3>(host_data, range) {}
	buffer(const DataType* host_data, range<3> range)
		: detail::buffer_<DataType, 3>(host_data, range) {}
	//buffer(storage<DataType>& store, range<3>);
	//buffer(buffer, index<3> base_index, range<3> sub_range);
	buffer(cl_mem mem_object, queue from_queue, event available_event)
		: detail::buffer_<DataType, 3>(mem_object, from_queue, available_event) {}
#else
	using detail::buffer_<DataType, 3>::buffer;
#endif
	buffer(size_t sizeX, size_t sizeY, size_t sizeZ)
		: buffer(range<3>{ sizeX, sizeY, sizeZ }) {}
	buffer(DataType* host_data, size_t sizeX, size_t sizeY, size_t sizeZ)
		: buffer(host_data, { sizeX, sizeY, sizeZ }) {}
};

} // namespace sycl
} // namespace cl
