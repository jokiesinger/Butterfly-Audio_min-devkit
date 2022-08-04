#pragma once


#include <vector>
#include <array>

namespace Butterfly {


/// @brief Data structure that allows adding and removing items according to ids which do not change
///        when other items are added or removed. Also no allocation or deallocation is performed
///        except during construction. The capacity is fixed and set at construction.
///
///
///        Let n be the capacity (internal size) and m the size (number of added elements).
///        Then the runtime cost for these operations is:
///        - add: O(1) (no allocation, may fail when capacity is exceeded)
///        - remove: O(m) (only for searching the correct id, at most 1 id swap is performed, no destruction)
///        - iterating over all added elements: O(m)
///
///        A std::vector which holds objects that carry a flag that indicates if they are active
///        or inactive (removed) however has runtimes of
///        - add: O(1) (although may allocate when capacity is exceeded)
///        - remove: O(n) (assignment for all objects after the removed)
///        - iterating over all active: O(n)
///
///        The data is stored in a dense representation (std::array) at a location that
///        matches its id.
///
///        Requirements on data type T:
///         - default-constructable
///
/// @tparam T Value type. 
/// @tparam capacity Final capacity of container. 
template<class T, size_t capacity>
class StableIDArray
{
public:
	using value_type = T;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using size_type = size_t;
	using id_type = size_t;
	using difference_type = ptrdiff_t;

	using value_container = std::array<value_type, capacity>;
	using index_container = std::array<size_type, capacity>;

	template<class U>
	class redirecting_iterator
	{
	public:
		using iterator_category = std::random_access_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using size_type = size_t;
		using value_type = std::remove_const_t<U>;
		using pointer = U*;
		using reference = U&;

		constexpr explicit redirecting_iterator(value_container& vc, index_container& ic, size_type offset = 0) noexcept : vc(vc), ic(ic), index{ offset } {}

		constexpr reference operator*() const noexcept { return vc[ic[index]]; }
		constexpr pointer operator->() const noexcept { return &vc[ic[index]]; }
		constexpr redirecting_iterator& operator++() noexcept {
			++index;
			return *this;
		}
		constexpr redirecting_iterator operator++(int) noexcept {
			redirecting_iterator tmp = *this;
			++index;
			return tmp;
		}
		constexpr redirecting_iterator& operator--() noexcept {
			--index;
			return *this;
		}
		constexpr redirecting_iterator operator--(int) noexcept {
			redirecting_iterator tmp = *this;
			--index;
			return tmp;
		}
		constexpr redirecting_iterator& operator+=(const difference_type offset) noexcept {
			index += offset;
			return *this;
		}
		constexpr redirecting_iterator operator+(const difference_type offset) const noexcept {
			redirecting_iterator tmp = *this;
			return tmp += offset;
		}
		constexpr redirecting_iterator& operator-=(const difference_type offset) noexcept {
			index -= offset;
			return *this;
		}
		constexpr redirecting_iterator operator-(const difference_type offset) const noexcept {
			redirecting_iterator tmp = *this;
			return tmp -= offset;
		}
		constexpr difference_type operator-(const redirecting_iterator& other) const noexcept { return index - other.index; }
		constexpr reference operator[](const difference_type offset) const noexcept { return vc[ic[index + offset]]; }
		// constexpr std::strong_ordering operator<=>(const iterator& right) const noexcept { return ptr <=> right.ptr; }
		constexpr friend bool operator==(const redirecting_iterator& a, const redirecting_iterator& b) noexcept { return a.index == b.index; }
		constexpr friend bool operator!=(const redirecting_iterator& a, const redirecting_iterator& b) noexcept { return a.index != b.index; }

	private:
		value_container& vc;
		index_container& ic;
		size_type index;
	};


	using iterator = redirecting_iterator<T>;
	using const_iterator = redirecting_iterator<const T>;



	constexpr StableIDArray() { initialize_indices(); }
	constexpr StableIDArray(const T& val) {
		initialize_indices();
		std::fill(values.begin(), values.end(), val);
	}

	constexpr const_iterator begin() const { return const_iterator{ values, value_indices, 0 }; }
	constexpr iterator begin() { return iterator{ values, value_indices, 0 }; }
	constexpr const_iterator end() const { return const_iterator{ values, value_indices, size_ }; }
	constexpr iterator end() { return iterator{ values, value_indices, size_ }; }

	constexpr T& operator[](id_type id) { return values[id]; }
	constexpr const T& operator[](id_type id) const { return values[id]; }

	constexpr size_type size() const { return size_; }
	constexpr size_type capacity() const { return values.size(); }

	constexpr id_type get_next_id() const { return size_ >= values.size() ? invalid_id : value_indices[size_]; }

	// returns id of connection
	constexpr id_type add(const T& value) {
		if (size_ >= values.size()) return invalid_id;
		const auto new_id = value_indices[size_];
		values[new_id] = value;
		size_++;
		return new_id;
	}

	constexpr void insert(const T& value, id_type id) {
		values[id] = value;
		const auto index = find_index(id);
		if (index >= size_) { // not in "active" range
			if (index != size_) {
				std::swap(value_indices[index], value_indices[size_]);
			}
			size_++;
		}
	}

	constexpr bool remove(id_type id) {
		if (id < values.size() && size_ >= 1) {
			const auto last_id = size_ - 1;
			const auto index = find_index(id);
			if (index == invalid_id || index > last_id) return false;
			if (last_id > index) {
				std::swap(value_indices[index], value_indices[last_id]);
			}
			size_--;
			return true;
		}
		return false;
	}

	static constexpr id_type invalid_id = static_cast<id_type>(-1);

protected:
	constexpr T& get_by_index(size_type index) { return values[value_indices[index]]; }
	constexpr const T& get_by_index(size_type index) const { return values[value_indices[index]]; }
	constexpr id_type get_id(size_type index) const { return value_indices[index]; }

	constexpr size_type find_index(id_type id) const {
		size_type i{ 0 };
		for (const auto c : value_indices) {
			if (c == id) return i;
			i++;
		}
		return invalid_id;
	}

private:
	constexpr void initialize_indices() {
		for (size_t i = 0; i < values.size(); i++)
			value_indices[i] = i;
	}

	// Preallocated buffer of values. These are not moved around and need to keep their addresses
	value_container values{};
	// Pointers to the items of values (see above). These can be moved around and swapped.
	index_container value_indices{};
	size_t size_{};
};

}
