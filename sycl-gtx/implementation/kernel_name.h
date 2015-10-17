namespace cl {
namespace sycl {

namespace detail {

class kernel_name {
private:
	static size_t current_count;

	template <class T>
	struct namer {
		static size_t id;
		static bool is_set;
		static void set() {
			if(!is_set) {
				id = current_count++;
				is_set = true;
			}
		}
	};

public:
	template <class T>
	static size_t get() {
		namer<T>::set();
		return namer<T>::id;
	}
};

size_t kernel_name::current_count = 0;
template <class T>
size_t kernel_name::namer<T>::id = 0;
template <class T>
bool kernel_name::namer<T>::is_set = false;

} // namespace detail

} // namespace sycl
} // namespace cl