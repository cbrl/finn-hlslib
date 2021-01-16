#pragma once

#include <array>
#include <cstdint>
#include <cassert>
#include <type_traits>
#include <utility>

#include "fpga_sparse_set.h"
#include "../util.h"
#include "ap_array.h"


template<typename ResourcePoolT, bool ConstIter>
class resource_pool_iterator {
	friend ResourcePoolT;

	using container_type = typename std::conditional<
		ConstIter,
		const typename ResourcePoolT::container_type,
		typename ResourcePoolT::container_type
	>::type;

public:

	using difference_type   = typename ResourcePoolT::difference_type;
	using size_type         = typename ResourcePoolT::size_type;
	using value_type        = typename std::conditional<ConstIter, const typename container_type::value_type, typename container_type::value_type>::type;
	using pointer           = typename std::conditional<ConstIter, typename container_type::const_pointer, typename container_type::pointer>::type;
	using reference         = typename std::conditional<ConstIter, typename container_type::const_reference, typename container_type::reference>::type;
	using iterator_category = std::random_access_iterator_tag;

private:

	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	resource_pool_iterator(difference_type idx) : index(idx) {
		#pragma HLS inline
	}

public:

	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	resource_pool_iterator() = default;
	resource_pool_iterator(const resource_pool_iterator&) = default;
	resource_pool_iterator(resource_pool_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Destructor
	//----------------------------------------------------------------------------------
	~resource_pool_iterator() = default;

	//----------------------------------------------------------------------------------
	// Operators - Assignment
	//----------------------------------------------------------------------------------
	resource_pool_iterator& operator=(const resource_pool_iterator&) = default;
	resource_pool_iterator& operator=(resource_pool_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Operators - Access
	//----------------------------------------------------------------------------------
	reference access(ResourcePoolT& pool) const {
		#pragma HLS inline
		const auto pos = static_cast<size_type>(index - 1);
		return pool.resources[pos];
	}

	reference access(ResourcePoolT& pool, difference_type offset) const {
		#pragma HLS inline
		const auto pos = static_cast<size_type>(index - offset - 1);
		return pool.resources[pos];
	}

	//----------------------------------------------------------------------------------
	// Operators - Arithmetic
	//----------------------------------------------------------------------------------
	resource_pool_iterator operator+(difference_type value) const noexcept {
		#pragma HLS inline
		return resource_pool_iterator{index - value};
	}

	resource_pool_iterator& operator++() noexcept {
		#pragma HLS inline
		--index;
		return *this;
	}

	resource_pool_iterator operator++(int) noexcept {
		#pragma HLS inline
		resource_pool_iterator old = *this;
		++(*this);
		return old;
	}

	resource_pool_iterator& operator+=(difference_type value) noexcept {
		#pragma HLS inline
		index -= value;
		return *this;
	}

	resource_pool_iterator operator-(difference_type value) const noexcept {
		#pragma HLS inline
		return (*this + -value);
	}

	difference_type operator-(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return other.index - index;
	}

	resource_pool_iterator& operator--() noexcept {
		#pragma HLS inline
		++index;
		return *this;
	}

	resource_pool_iterator operator--(int) noexcept {
		#pragma HLS inline
		resource_pool_iterator old = *this;
		--(*this);
		return old;
	}

	resource_pool_iterator& operator-=(difference_type value) noexcept {
		#pragma HLS inline
		return (*this += -value);
	}

	//----------------------------------------------------------------------------------
	// Operators - Equality
	//----------------------------------------------------------------------------------
	bool operator==(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return other.index == index;
	}

	bool operator!=(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this == other);
	}

	bool operator<(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return index > other.index;
	}

	bool operator>(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return index < other.index;
	}

	bool operator<=(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this > other);
	}

	bool operator>=(const resource_pool_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this < other);
	}

private:

	difference_type index;
};


template<typename HandleT, typename ResourceT, size_t Size>
class ResourcePool {
	template <typename, bool>
	friend class resource_pool_iterator;

	using sparse_set_type = SparseSet<HandleT, (1ull << HandleT::width), Size>;
	using container_type  = ap_array<ResourceT, Size>;

public:

	using handle_type     = HandleT;
	using value_type      = ResourceT;
	using pointer         = ResourceT*;
	using const_pointer   = const ResourceT*;
	using reference       = ResourceT&;
	using const_reference = const ResourceT&;
	using size_type       = typename sparse_set_type::sparse_index;
	using difference_type = typename sparse_set_type::sparse_difference_type;

	using iterator       = resource_pool_iterator<ResourcePool<HandleT, ResourceT, Size>, false>;
	using const_iterator = resource_pool_iterator<ResourcePool<HandleT, ResourceT, Size>, true>;

	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	ResourcePool() = default;
	ResourcePool(const ResourcePool&) = default;
	ResourcePool(ResourcePool&&) = default;


	//----------------------------------------------------------------------------------
	// Destructor
	//----------------------------------------------------------------------------------
	~ResourcePool() = default;


	//----------------------------------------------------------------------------------
	// Operators
	//----------------------------------------------------------------------------------
	ResourcePool& operator=(const ResourcePool&) = default;
	ResourcePool& operator=(ResourcePool&&) = default;


	//----------------------------------------------------------------------------------
	// Member Functions - Modifiers
	//----------------------------------------------------------------------------------
	template<typename... ArgsT>
	std::pair<iterator, bool> emplace(handle_type resource_idx, ArgsT&& ... args) {
		if (contains(resource_idx)) {
			return {iterator(sparse_set.index_of(resource_idx)+1), false};
		}

		if (sparse_set.size() < sparse_set.capacity()) {
			sparse_set.insert(resource_idx);
			resources[sparse_set.size()-1] = ResourceT(std::forward<ArgsT>(args)...);
			return {iterator(sparse_set.size()), true};
		}
		else {
			return {end(), false};
		}
	}

	std::pair<iterator, bool> emplace_empty(handle_type resource_idx) {
		if (contains(resource_idx)) {
			return {iterator(sparse_set.index_of(resource_idx)+1), false};
		}

		if (sparse_set.size() < sparse_set.capacity()) {
			sparse_set.insert(resource_idx);
			return {iterator(sparse_set.size()), true};
		}
		else {
			return {end(), false};
		}
	}

	void erase(handle_type resource_idx) {
		if (!contains(resource_idx)) return;

		auto&& back = std::move(resources[sparse_set.size()-1]);
		resources[sparse_set.index_of(resource_idx)] = std::move(back);
		sparse_set.erase(resource_idx);
	}

	void clear() noexcept {
		#pragma HLS inline
		sparse_set.clear();
		//resources.clear();
	}


	//----------------------------------------------------------------------------------
	// Member Functions - Access
	//----------------------------------------------------------------------------------
	bool contains(handle_type resource_idx) const noexcept {
		#pragma HLS inline
		return sparse_set.contains(resource_idx);
	}

	reference at(handle_type resource_idx) {
		#pragma HLS inline
		assert(contains(resource_idx));
		const auto idx = sparse_set.index_of(resource_idx);
		return resources[idx];
	}

	const_reference at(handle_type resource_idx) const {
		#pragma HLS inline
		assert(contains(resource_idx));
		const auto idx = sparse_set.index_of(resource_idx);
		return resources[idx];
	}

	pointer data() noexcept {
		#pragma HLS inline
		return resources.data();
	}

	const_pointer data() const noexcept {
		#pragma HLS inline
		return resources.data();
	}

	// Get a const reference to the container holding the resource handles
	const sparse_set_type& handles() const noexcept {
		#pragma HLS inline
		return sparse_set;
	}


	//----------------------------------------------------------------------------------
	// Member Functions - Iterators
	//----------------------------------------------------------------------------------
	iterator begin() noexcept {
		#pragma HLS inline
		const auto end = static_cast<typename iterator::difference_type>(sparse_set.size());
		return iterator(end);
	}

	const_iterator begin() const noexcept {
		#pragma HLS inline
		const auto end = static_cast<typename iterator::difference_type>(sparse_set.size());
		return const_iterator(end);
	}

	const_iterator cbegin() const noexcept {
		#pragma HLS inline
		const auto end = static_cast<typename iterator::difference_type>(sparse_set.size());
		return const_iterator(end);
	}

	iterator end() noexcept {
		#pragma HLS inline
		return iterator(0);
	}

	const_iterator end() const noexcept {
		#pragma HLS inline
		return const_iterator(0);
	}

	const_iterator cend() const noexcept {
		#pragma HLS inline
		return const_iterator(0);
	}


	//----------------------------------------------------------------------------------
	// Member Functions - Capacity
	//----------------------------------------------------------------------------------
	bool empty() const noexcept {
		#pragma HLS inline
		return sparse_set.empty();
	}

	size_type size() const noexcept {
		#pragma HLS inline
		return sparse_set.size();
	}


private:

	//----------------------------------------------------------------------------------
	// Member Variables
	//----------------------------------------------------------------------------------
	sparse_set_type sparse_set;
	container_type  resources;
};
