#pragma once

#include "../specification/accessor/buffer.h"
#include "../specification/command_group.h"
#include "../common.h"
#include "../counter.h"
#include "../debug.h"
#include <unordered_map>


namespace cl {
namespace sycl {

// Forward declarations
class kernel;
class program;
class queue;

namespace detail {

// Forward declaration
class issue_command;

namespace kernel_ {

// Forward declaration
template<class Input>
struct constructor;


class source : protected counter<source> {
private:
	struct buf_info {
		buffer_access acc;
		string_class resource_name;
		string_class type_name;
		size_t size;
	};

	static const string_class resource_name_root;
	SYCL_THREAD_LOCAL static int num_resources;

	string_class tab_offset;

	string_class kernel_name;
	vector_class<string_class> lines;
	std::unordered_map<void*, buf_info> resources;

	// TODO: Multithreading support
	SYCL_THREAD_LOCAL static source* scope;

	template<class Input>
	friend struct constructor;
	friend class ::cl::sycl::detail::issue_command;

	string_class generate_accessor_list() const;

	static void enter(source& src);
	static source exit(source& src);

public:
	source()
		: tab_offset("\t"),
		kernel_name(string_class("_sycl_kernel_") + std::to_string(get_count_id())) {}

	static bool in_scope();

	string_class get_code() const;
	string_class get_kernel_name() const;
	
	void init_kernel(program& p, shared_ptr_class<kernel> kern);

	template <typename DataType, int dimensions, access::mode mode, access::target target>
	static string_class register_resource(const accessor_core<DataType, dimensions, mode, target>& acc) {
		if(scope == nullptr) {
			//error::report(error::code::NOT_IN_KERNEL_SCOPE);
			return "";
		}

		string_class resource_name;
		auto buf = (buffer<DataType, dimensions>*) acc.resource();
		auto it = scope->resources.find(buf);

		if(it == scope->resources.end()) {
			resource_name = resource_name_root + std::to_string(++num_resources);
			scope->resources[buf] = {
				{ buf, mode, target },
				resource_name,
				detail::type_string<DataType>::get() + '*',
				acc.argument_size()
			};
		}
		else {
			resource_name = it->second.resource_name;
		}

		return resource_name;
	}

	template <bool auto_end = true>
	static void add(string_class line) {
		scope->lines.push_back(scope->tab_offset + line + (auto_end ? ';' : ' '));
	}

	static void add_curlies() {
		add<false>("{");
		scope->tab_offset.push_back('\t');
	}
	static void remove_curlies() {
		scope->tab_offset.pop_back();
		add<false>("}");
	}

	static string_class get_name(access::target target);
};

} // namespace kernel_
} // namespace detail

} // namespace sycl
} // namespace cl
