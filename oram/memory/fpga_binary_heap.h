#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <ap_int.h>
#include "../util.h"
#include "ap_array.h"


template<typename MapT, bool ConstIter>
class binary_heap_iterator {
	friend MapT;

	using map_type = typename std::conditional<ConstIter, const MapT, MapT>::type;
	using container_type = typename std::conditional<ConstIter, const typename map_type::container_type, typename map_type::container_type>::type;

public:
	using difference_type   = typename map_type::difference_type;
	using size_type         = typename map_type::size_type;
	using value_type        = typename std::conditional<ConstIter, const typename map_type::value_type, typename map_type::value_type>::type;
	using pointer           = typename std::conditional<ConstIter, typename map_type::const_pointer, typename map_type::pointer>::type;
	using reference         = typename std::conditional<ConstIter, typename map_type::const_reference, typename map_type::reference>::type;

private:
	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	binary_heap_iterator(difference_type idx) : node(idx) {
		#pragma HLS inline
	}

public:
	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	binary_heap_iterator() = default;
	binary_heap_iterator(const binary_heap_iterator&) = default;
	binary_heap_iterator(binary_heap_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Destructor
	//----------------------------------------------------------------------------------
	~binary_heap_iterator() = default;

	//----------------------------------------------------------------------------------
	// Operators - Assignment
	//----------------------------------------------------------------------------------
	binary_heap_iterator& operator=(const binary_heap_iterator&) = default;
	binary_heap_iterator& operator=(binary_heap_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Operators - Access
	//----------------------------------------------------------------------------------
	reference access(map_type& tree) const {
		#pragma HLS inline
		return tree.data[node].value();
	}

	//----------------------------------------------------------------------------------
	// Operators - Arithmetic
	//----------------------------------------------------------------------------------
	binary_heap_iterator& increment(map_type& tree) {
		if (node >= map_type::num_elements) {
			node = map_type::num_elements;
			return *this;
		}

		const size_type right = (node * 2) + 2;
		if (tree.is_invalid_leaf(right)) {
			while (true) {
				const size_type parent = tree.get_parent(node);

				if (node == parent) { //get_parent returns root node if given the root node
					node = map_type::num_elements;
					break;
				}
				if (tree.get_left_child(parent) == node) {
					node = parent;
					break;
				}

				node = parent;
			}
		}
		else {
			node = tree.find_min(right);
		}

		return *this;
	}

	//----------------------------------------------------------------------------------
	// Operators - Equality
	//----------------------------------------------------------------------------------
	bool operator==(const binary_heap_iterator& other) const noexcept {
		#pragma HLS inline
		return other.node == node;
	}

	bool operator!=(const binary_heap_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this == other);
	}

private:

	difference_type node = map_type::num_elements;
};


template<typename KeyT, typename ValueT, uint8_t Height, typename CompareT = std::less<KeyT>>
class BinaryHeap {
	template <typename, bool>
	friend class binary_heap_iterator;

	static constexpr size_t num_elements = (1ull << (Height + 1)) - 1;

public:

	using node_id = ap_uint<util::ceil_int_log2((num_elements*2) + 2)>; //excess ensures get_x_child() functions won't overflow

	using key_type        = KeyT;
	using mapped_type     = ValueT;
	using value_type      = std::pair<KeyT, ValueT>;
	using pointer         = value_type*;
	using const_pointer   = const value_type*;
	using reference       = value_type&;
	using const_reference = const value_type&;
	using size_type       = node_id;
	using difference_type = ap_int<node_id::width + 1>;

	using iterator = binary_heap_iterator<BinaryHeap<KeyT, ValueT, Height, CompareT>, false>;
	using const_iterator = binary_heap_iterator<BinaryHeap<KeyT, ValueT, Height, CompareT>, true>;

private:

	struct Node {
		const key_type& key() const noexcept {
			#pragma HLS inline
			return kv_pair.first;
		}
		key_type& key() noexcept {
			#pragma HLS inline
			return kv_pair.first;
		}

		const mapped_type& mapped() const noexcept {
			#pragma HLS inline
			return kv_pair.second;
		}
		mapped_type& mapped() noexcept {
			#pragma HLS inline
			return kv_pair.second;
		}

		const value_type& value() const noexcept {
			#pragma HLS inline
			return kv_pair;
		}
		value_type& value() noexcept {
			#pragma HLS inline
			return kv_pair;
		}

		ap_uint<1> valid = false;
		value_type kv_pair;
	};

	struct NodeCompare {
		bool operator()(const Node& lhs, const Node& rhs) {
			#pragma HLS inline
			return CompareT()(lhs.key(), rhs.key());
		}
	};

	using container_type = ap_array<Node, num_elements>;

public:

	BinaryHeap() {
		for (size_type i = 0; i < num_elements; ++i) {
			data[i].valid = false;
		}
	}

	std::pair<iterator, bool> insert(const value_type& value) {
		const size_type leaf = find_insertion_spot(value.first);

		if (leaf >= num_elements) {
			return {end(), false};
		}

		// Node is invalid or keys are equal
		auto& node = data[leaf];
		if (!node.valid) {
			node.valid = true;
			node.key() = value.first;
			node.mapped() = value.second;
			return {make_iterator(leaf), true};
		}
		else {
			return {make_iterator(leaf), false};
		}
	}

	template<typename... ArgsT>
	std::pair<iterator, bool> emplace(const key_type& key, ArgsT&&... val_args) {
		const size_type leaf = find_insertion_spot(key);

		if (leaf >= num_elements) {
			return {end(), false};
		}

		// Node is invalid or keys are equal
		auto& node = data[leaf];
		if (!node.valid) {
			node.valid = true;
            node.key() = key;
			node.mapped() = ValueT(val_args...);
			return {make_iterator(leaf), true};
		}
		else {
			return {make_iterator(leaf), false};
		}
	}

	std::pair<iterator, bool> emplace_empty(const key_type& key) {
		const size_type leaf = find_insertion_spot(key);

		if (leaf >= num_elements) {
			return {end(), false};
		}

		auto& node = data[leaf];

		// Node is invalid or keys are equal
		if (!node.valid) {
			node.valid = true;
            node.key() = key;
			return {make_iterator(leaf), false};
		}
		else {
			return {make_iterator(leaf), false};
		}
	}

	void erase(const key_type& key) {
		const size_type leaf = find_leaf(key);
		if (leaf >= num_elements) {
			return;
		}

		auto& node = data[leaf];

		// Only need to test for valid child nodes if this isn't the max height
		if (leaf < ((1ull << Height)-1)) {
			const size_type left_child = get_left_child(leaf);
			const size_type right_child = get_right_child(leaf);

			const bool has_left = !is_invalid_leaf(left_child);
			const bool has_right = !is_invalid_leaf(right_child);

			if (has_left && has_right) { //two children
				const size_type successor = find_min(right_child);

				// Move the min node of the right child to the erased node
				auto& successor_node = data[successor];
				node = successor_node;
				successor_node.valid = false;

				// If the min node had its own children, then iteratively move them up to the
				// min node's old spot. The min node can only have a right child, otherwise it
				// wouldn't have been the minimum.
				const size_type successor_right_child = get_right_child(successor);
				if (!is_invalid_leaf(successor_right_child)) {
					iterative_move(successor_right_child, successor);
				}
				return;
			}
			else if (has_left) { //only left child
				iterative_move(left_child, leaf);
				return;
			}
			else if (has_right) { //only right child
				iterative_move(right_child, leaf);
				return;
			}
			else { //no children
				node.valid = false;
				return;
			}
		}
		else { //no children on nodes at max height
			node.valid = false;
			return;
		}
	}

	bool contains(const key_type& key) const {
		#pragma HLS inline
		return find_leaf(key) != num_elements;
	}

	mapped_type& at(const key_type& key) {
		#pragma HLS inline
		assert(contains(key));
		return data[find_leaf(key)].mapped();
	}

	const mapped_type& at(const key_type& key) const {
		#pragma HLS inline
		assert(contains(key));
		return data[find_leaf(key)].mapped();
	}

	iterator find(const key_type& key) {
		#pragma HLS inline
		return make_iterator(find_leaf(key));
	}

	const_iterator find(const key_type& key) const {
		#pragma HLS inline
		return make_const_iterator(find_leaf(key));
	}

	iterator begin() {
		#pragma HLS inline
		return make_iterator(find_min(0));
	}

	const_iterator begin() const {
		#pragma HLS inline
		return make_const_iterator(find_min(0));
	}

	iterator end() noexcept {
		#pragma HLS inline
		return make_iterator(num_elements);
	}

	const_iterator end() const noexcept {
		#pragma HLS inline
		return make_const_iterator(num_elements);
	}

private:

	// Find the first element that has a matching key
	size_type find_leaf(const key_type& key) const {
		#pragma HLS inline

		size_type leaf = 0;
		while ((leaf < num_elements) && !equal(key, data[leaf].key())) {
			leaf += less(key, data[leaf].key()) ? (leaf + 1) : (leaf + 2);
		}
		return (leaf < num_elements) ? leaf : num_elements;
	}

	// Find the first element that has a matching key or an invalid key
	size_type find_insertion_spot(const key_type& key) const {
		#pragma HLS inline

		size_type leaf = 0;
		while ((leaf < num_elements) && !equal(key, data[leaf].key()) && data[leaf].valid) {
			leaf += less(key, data[leaf].key()) ? (leaf + 1) : (leaf + 2);
		}
		return (leaf < num_elements) ? leaf : num_elements;
	}

	size_type find_min(size_type leaf) const {
		#pragma HLS inline

		if (is_invalid_leaf(leaf))
			return num_elements;

		size_type next_leaf = leaf;
		while (!is_invalid_leaf(next_leaf)) {
			leaf = next_leaf;
			next_leaf = (leaf * 2) + 1;
		}
		return leaf;
	}

	size_type find_max(size_type leaf) {
		#pragma HLS inline

		if (is_invalid_leaf(leaf))
			return num_elements;

		size_type next_leaf = leaf;
		while (!is_invalid_leaf(next_leaf)) {
			leaf = next_leaf;
			next_leaf = (leaf * 2) + 2;
		}
		return leaf;
	}

	// Move a node and all of its children.
	// Notes:
	//   - Won't mark untouched nodes as invalid, or rebalance the tree.
	//   - Will overwrite nodes that overlap with the source node's subtree.
	//   - This method is only intended for use with erase(), which makes extra checks
	//     to ensure nodes won't be orphaned and valid nodes won't be overwritten.
	void iterative_move(size_type from, size_type to) {
		size_type dest_start = to;
		size_type cur_dest = dest_start;

		size_type src_start = from;
		size_type cur_src = src_start;

		const size_type levels = Height - util::integer_log2(from + 1) + 1;

		// Move the nodes in the current height level to the corresponding destination
		for (size_type lvl = 0; lvl < levels; ++lvl) {
			const size_type nodes = 1ull << lvl;

			// Move each src node in the current level to the corresponding dest node
			for (size_type n = 0; n < nodes; ++n) {
				auto& src_node = data[cur_src];

				if (src_node.valid) {
					data[cur_dest] = src_node;
					src_node.valid = false;
				}

				++cur_dest;
				++cur_src;
			}

			// Set the src and dest node to the left child of the current src and dest node
			cur_dest = dest_start = get_left_child(dest_start);
			cur_src  = src_start  = get_left_child(src_start);
		}
	}

	iterator make_iterator(size_type leaf) noexcept {
		#pragma HLS inline
		return iterator{static_cast<difference_type>(leaf)};
	}

	const_iterator make_const_iterator(size_type leaf) const noexcept {
		#pragma HLS inline
		return const_iterator{static_cast<difference_type>(leaf)};
	}

	size_type get_parent(size_type leaf) const noexcept {
		#pragma HLS inline
		return (leaf == 0) ? 0 : (leaf - 1) / 2;
	}

	size_type get_left_child(size_type leaf) const noexcept {
		#pragma HLS inline
		return (leaf * 2) + 1;
	}

	size_type get_right_child(size_type leaf) const noexcept {
		#pragma HLS inline
		return (leaf * 2) + 2;
	}

	bool is_invalid_leaf(size_type leaf) const {
		#pragma HLS inline
		return (leaf >= num_elements) || (!data[leaf].valid);
	}

	bool less(const key_type& lhs, const key_type& rhs) const {
		#pragma HLS inline
		return CompareT()(lhs, rhs);
	}

	bool less(const Node& lhs, const Node& rhs) const {
		#pragma HLS inline
		return NodeCompare()(lhs, rhs);
	}

	bool equal(const key_type& lhs, const key_type& rhs) const {
		#pragma HLS inline
		return !less(lhs, rhs) && !less(rhs, lhs);
	}

	bool equal(const Node& lhs, const Node& rhs) const {
		#pragma HLS inline
		return !less(lhs, rhs) && !less(rhs, lhs);
	}


	container_type data;
};
