#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <type_traits>

#include <ap_int.h>
#include "../util.h"
#include "ap_array.h"


// The state of a sparse set should only be modified through its member
// functions, so the only iterator type is a const iterator. The underlying
// dense container is reverse iterated. This ensures that the current element
// can be deleted while iterating and no other elements will be skipped.
// However, elements added while iterating will not be covered.
template<typename SparseSetT>
class sparse_set_iterator {
	friend SparseSetT;

	using container_type = typename SparseSetT::dense_container_type;

public:

	using value_type        = typename SparseSetT::value_type;
	using difference_type   = typename SparseSetT::dense_difference_type;
	using const_pointer     = typename SparseSetT::const_pointer;
	using reference         = typename SparseSetT::reference;
	using const_reference   = typename SparseSetT::const_reference;
	using iterator_category = std::random_access_iterator_tag;

	using sparse_index      = typename SparseSetT::sparse_index;
	using dense_index       = typename SparseSetT::dense_index;


private:

	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	sparse_set_iterator(difference_type idx) : index(idx) {
		#pragma HLS inline
	}

public:

	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	sparse_set_iterator() = default;
	sparse_set_iterator(const sparse_set_iterator&) = default;
	sparse_set_iterator(sparse_set_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Destructors
	//----------------------------------------------------------------------------------
	~sparse_set_iterator() = default;

	//----------------------------------------------------------------------------------
	// Operators - Assignment
	//----------------------------------------------------------------------------------
	sparse_set_iterator& operator=(const sparse_set_iterator&) = default;
	sparse_set_iterator& operator=(sparse_set_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Operators - Access
	//----------------------------------------------------------------------------------
	const_reference access(const SparseSetT& set) const {
		#pragma HLS inline
		const auto pos = static_cast<dense_index>(index - 1);
		return set.dense[pos];
	}

	const_reference access(const SparseSetT& set, difference_type offset) const {
		#pragma HLS inline
		const auto pos = static_cast<dense_index>(index - offset - 1);
		return set.dense[pos];
	}

	//----------------------------------------------------------------------------------
	// Operators - Arithmetic
	//----------------------------------------------------------------------------------
	sparse_set_iterator& operator++() noexcept {
		#pragma HLS inline
		--index;
		return *this;
	}

	sparse_set_iterator operator++(int) noexcept {
		#pragma HLS inline
		sparse_set_iterator old = *this;
		++(*this);
		return old;
	}

	sparse_set_iterator operator+(difference_type value) const noexcept {
		#pragma HLS inline
		return sparse_set_iterator(index - value);
	}

	sparse_set_iterator& operator+=(difference_type value) noexcept {
		#pragma HLS inline
		index -= value;
		return *this;
	}

	sparse_set_iterator& operator--() noexcept {
		#pragma HLS inline
		++index;
		return *this;
	}

	sparse_set_iterator operator--(int) noexcept {
		#pragma HLS inline
		sparse_set_iterator old = *this;
		--(*this);
		return old;
	}

	sparse_set_iterator operator-(difference_type value) const noexcept {
		#pragma HLS inline
		return (*this + -value);
	}

	difference_type operator-(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return other.index - index;
	}

	sparse_set_iterator& operator-=(difference_type value) noexcept {
		#pragma HLS inline
		return (*this += -value);
	}

	//----------------------------------------------------------------------------------
	// Operators - Equality
	//----------------------------------------------------------------------------------
	bool operator==(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return other.index == index;
	}

	bool operator!=(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this == other);
	}

	bool operator<(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return index > other.index;
	}

	bool operator>(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return index < other.index;
	}

	bool operator<=(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this > other);
	}

	bool operator>=(const sparse_set_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this < other);
	}

private:

	difference_type index;
};


// T should be an unsigned integral type. Either a built-in integer type or an ap_[u]int type.
template<typename T, size_t SparseSize, size_t DenseSize = SparseSize>
class SparseSet {
	//static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value, "SparseSet only supports unsigned integral types");
	static_assert(SparseSize >= DenseSize, "SparseSize must be >= DenseSize");

	template<typename>
	friend class sparse_set_iterator;

public:
	using value_type             = T;
	using pointer                = T*;
	using const_pointer          = const T*;
	using reference              = T&;
	using const_reference        = const T&;

	using sparse_index = ap_uint<util::ceil_int_log2(SparseSize)>;
	using dense_index  = ap_uint<util::ceil_int_log2(DenseSize)>;

	using sparse_difference_type = ap_int<sparse_index::width + 1>;
	using dense_difference_type  = ap_int<dense_index::width + 1>;

	using dense_container_type  = ap_array<T, DenseSize>;
	using sparse_container_type = ap_array<dense_index, SparseSize>;

	using iterator = sparse_set_iterator<SparseSet<T, SparseSize, DenseSize>>;


	//----------------------------------------------------------------------------------
	// Member Functions - Access
	//----------------------------------------------------------------------------------
	bool contains(sparse_index val) const noexcept {
		#pragma HLS inline
		return val < sparse.size()      &&
		       sparse[val] < dense_size &&
		       dense[sparse[val]] == val;
	}

	dense_index index_of(sparse_index val) const noexcept {
		#pragma HLS inline
		assert(contains(val));
		return sparse[val];
	}

	pointer data() noexcept {
		#pragma HLS inline
		return dense.data();
	}

	const_pointer data() const noexcept {
		#pragma HLS inline
		return dense.data();
	}


	//----------------------------------------------------------------------------------
	// Member Functions - Iterators
	//----------------------------------------------------------------------------------
	iterator begin() const noexcept {
		#pragma HLS inline
		const auto end = static_cast<typename iterator::difference_type>(dense_size);
		return iterator(end);
	}

	iterator cbegin() const noexcept {
		#pragma HLS inline
		const auto end = static_cast<typename iterator::difference_type>(dense_size);
		return iterator(end);
	}

	iterator end() const noexcept {
		#pragma HLS inline
		return iterator(0);
	}

	iterator cend() const noexcept {
		#pragma HLS inline
		return iterator(0);
	}


	//----------------------------------------------------------------------------------
	// Member Functions - Capacity
	//----------------------------------------------------------------------------------
	bool empty() const noexcept {
		#pragma HLS inline
		return dense_size == 0;
	}

	sparse_index size() const noexcept {
		#pragma HLS inline
		return dense_size;
	}

	static constexpr sparse_index capacity() noexcept {
		return DenseSize;
	}

	//----------------------------------------------------------------------------------
	// Member Functions - Modifiers
	//----------------------------------------------------------------------------------
	void clear() noexcept {
		#pragma HLS inline
		dense_size = 0;
	}

	// Insert the given value into the sparse set. If the set's capacity is less than
	// the value, then it will be resized to the value + 1;
	void insert(sparse_index val) {
		#pragma HLS inline
		if (val >= sparse.size()) return;

		if (!contains(val)) {
			sparse[val] = dense_size;
			dense[dense_size] = val;
			++dense_size;
		}
	}

	void erase(sparse_index val) {
		#pragma HLS inline
		if (contains(val)) {
			dense[sparse[val]] = dense[dense_size-1];
			sparse[dense[dense_size-1]] = sparse[val];
			--dense_size;
		}
	}

	void swap(SparseSet& other) noexcept {
		#pragma HLS inline
		dense.swap(other.dense);
		sparse.swap(other.sparse);
	}

private:

	size_t dense_size = 0;
	dense_container_type dense;
	sparse_container_type sparse;
};
