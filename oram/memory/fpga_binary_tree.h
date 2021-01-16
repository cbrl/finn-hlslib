#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include <ap_int.h>
#include "../util.h"
#include "ap_array.h"


template<typename TreeT, bool ConstIter>
class binary_tree_iterator {
	friend TreeT;

	using tree_type = typename std::conditional<ConstIter, const TreeT, TreeT>::type;
	using container_type = typename std::conditional<ConstIter, const typename tree_type::container_type, typename tree_type::container_type>::type;

public:
	using difference_type   = typename tree_type::difference_type;
	using size_type         = typename tree_type::size_type;
	using value_type        = typename std::conditional<ConstIter, const typename tree_type::value_type, typename tree_type::value_type>::type;
	using pointer           = typename std::conditional<ConstIter, typename tree_type::const_pointer, typename tree_type::pointer>::type;
	using reference         = typename std::conditional<ConstIter, typename tree_type::const_reference, typename tree_type::reference>::type;

private:
	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	binary_tree_iterator(difference_type idx) : node(idx) {
		#pragma HLS inline
	}

public:
	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	binary_tree_iterator() = default;
	binary_tree_iterator(const binary_tree_iterator&) = default;
	binary_tree_iterator(binary_tree_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Destructor
	//----------------------------------------------------------------------------------
	~binary_tree_iterator() = default;

	//----------------------------------------------------------------------------------
	// Operators - Assignment
	//----------------------------------------------------------------------------------
	binary_tree_iterator& operator=(const binary_tree_iterator&) = default;
	binary_tree_iterator& operator=(binary_tree_iterator&&) noexcept = default;

	//----------------------------------------------------------------------------------
	// Operators - Access
	//----------------------------------------------------------------------------------
	reference access(tree_type& tree) const {
		#pragma HLS inline
		return tree.nodes[node].value();
	}

	//----------------------------------------------------------------------------------
	// Operators - Arithmetic
	//----------------------------------------------------------------------------------
	binary_tree_iterator& increment(tree_type& tree) {
		if (node >= tree_type::invalid_node) {
			node = tree_type::invalid_node;
			return *this;
		}

		auto& nodes = tree.nodes;

		if (tree.is_invalid_node(nodes[node].right)) {
			while (true) {
				auto& node_ref = nodes[node];

				if (node_ref.parent == tree_type::invalid_node) {
					node = tree_type::invalid_node;
					break;
				}
				if (nodes[node_ref.parent].left == node) {
					node = node_ref.parent;
					break;
				}
				node = node_ref.parent;
			}
		}
		else {
			node = tree.find_min(nodes[node].right);
		}

		return *this;
	}

	//----------------------------------------------------------------------------------
	// Operators - Equality
	//----------------------------------------------------------------------------------
	bool operator==(const binary_tree_iterator& other) const noexcept {
		#pragma HLS inline
		return other.node == node;
	}

	bool operator!=(const binary_tree_iterator& other) const noexcept {
		#pragma HLS inline
		return !(*this == other);
	}

private:

	difference_type node;
};


template<typename KeyT, typename ValueT, size_t NodeCount, typename CompareT = std::less<KeyT>>
class BinaryTree {
	template<typename, bool>
	friend class binary_tree_iterator;

	// The minimum width integer required to represent the number of nodes in the container
	using node_id = ap_uint<util::ceil_int_log2(NodeCount + 1)>;

public:

	using key_type        = KeyT;
	using mapped_type     = ValueT;
	using value_type      = std::pair<KeyT, ValueT>;
	using pointer         = value_type*;
	using const_pointer   = const value_type*;
	using reference       = value_type&;
	using const_reference = const value_type&;
	using size_type       = node_id;
	using difference_type = ap_int<node_id::width + 1>;
	
	using iterator = binary_tree_iterator<BinaryTree<KeyT, ValueT, NodeCount, CompareT>, false>;
	using const_iterator = binary_tree_iterator<BinaryTree<KeyT, ValueT, NodeCount, CompareT>, true>;

private:

	static constexpr size_t invalid_node = NodeCount;

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
		node_id parent = invalid_node;
		node_id left   = invalid_node;
		node_id right  = invalid_node;
		value_type kv_pair;
	};

	struct NodeCompare {
		bool operator()(const Node& lhs, const Node& rhs) {
			#pragma HLS inline
			return CompareT()(lhs.key(), rhs.key());
		}
	};

	using container_type  = ap_array<Node, NodeCount>;

public:

	BinaryTree() {
		for (node_id i = 0; i < free_nodes.size(); ++i) {
			push_free(i);
		}
	}

	std::pair<iterator, bool> insert(const value_type& value) {
		#pragma HLS inline
		auto it = setup_new_node(value.first);
		if (it.second) { //if a new node was created, set the value of the pair.
			it.first.access(*this).second = value.second;
		}

		return it;
	}

	template<typename... ArgsT>
	std::pair<iterator, bool> emplace(const key_type& key, ArgsT&&... args) {
		#pragma HLS inline
		auto it = setup_new_node(key);
		if (it.second) { //if a new node was created, set the value of the pair.
			it.first.access(*this).second = mapped_type(std::forward<ArgsT>(args)...);
		}

		return it;
	}

	std::pair<iterator, bool> emplace_empty(const key_type& key) {
		#pragma HLS inline
		return setup_new_node(key);
	}

	void erase(const key_type& key) {
		const auto id = find_exact(key);
		if (is_invalid_node(id)) return;

		auto& node = nodes[id];

		const bool has_left  = !is_invalid_node(node.left);
		const bool has_right = !is_invalid_node(node.right);

		if (has_left && has_right) {
			const auto successor = find_min(node.right);
			auto& successor_node = nodes[successor];

			// Store successor relationship before it's deleted
			const auto old_successor_parent = successor_node.parent;
			const auto old_successor_right  = successor_node.right;

			// Move successor to old node's spot
			move_node(successor, id, false, false);

			// If the successor node has children, then move the right subtree of the successor to the successor's old spot.
			if (!is_invalid_node(old_successor_right)) {
				auto& old_successor_right_node = nodes[old_successor_right];

				// Setup a temporary node in the successor's old spot
				node_id temp_successor = setup_new_node(old_successor_right_node.key()).first.node;
				auto& temp_successor_node = nodes[temp_successor];
				auto& temp_successor_parent_node = nodes[temp_successor_node.parent];

				// Move the old successor's right subtree into the temp node's spot
				old_successor_right_node.parent = temp_successor_node.parent;
				if (temp_successor_parent_node.left == temp_successor) {
					temp_successor_parent_node.left = old_successor_right;
				}
				else {
					temp_successor_parent_node.right = old_successor_right;
				}
				push_free(temp_successor);
			}
		}
		else if (has_left) {
			move_node(node.left, id, true, true);
		}
		else if (has_right) {
			move_node(node.right, id, true, true);
		}
		else { //no children
			if (id == root) {
				root = invalid_node;
			}
			else {
				auto& parent = nodes[node.parent];
				if (parent.left == id) {
					parent.left = invalid_node;
				}
				else {
					parent.right = invalid_node;
				}
			}
			push_free(id);
		}
	}

	bool contains(const key_type& key) const {
		#pragma HLS inline
		return find_exact(key) != invalid_node;
	}

	mapped_type& at(const key_type& key) {
		#pragma HLS inline
		assert(contains(key));
		return nodes[find_exact(key)].mapped();
	}

	const mapped_type& at(const key_type& key) const {
		#pragma HLS inline
		assert(contains(key));
		return nodes[find_exact(key)].mapped();
	}

	iterator find(const key_type& key) {
		#pragma HLS inline
		return make_iterator(find_exact(key));
	}

	const_iterator find(const key_type& key) const {
		#pragma HLS inline
		return make_const_iterator(find_exact(key));
	}

	iterator begin() {
		#pragma HLS inline
		return make_iterator(find_min(root));
	}

	const_iterator begin() const {
		#pragma HLS inline
		return make_const_iterator(find_min(root));
	}

	iterator end() noexcept {
		#pragma HLS inline
		return make_iterator(invalid_node);
	}

	const_iterator end() const noexcept {
		#pragma HLS inline
		return make_const_iterator(invalid_node);
	}

private:

	iterator make_iterator(size_type leaf) {
		#pragma HLS inline
		return iterator{static_cast<difference_type>(leaf)};
	}

	const_iterator make_const_iterator(size_type leaf) const {
		#pragma HLS inline
		return const_iterator{static_cast<difference_type>(leaf)};
	}

	std::pair<iterator, bool> setup_new_node(const key_type& key) {
		if (free_count == 0) {
			return {end(), false};
		}

		// Special case when the tree is completely emtpy
		if (is_invalid_node(root)) {
			const auto root_id = pop_free();
			auto& root_node = nodes[root_id];
			root = root_id;
			root_node.key() = key;
			return {make_iterator(root), true};
		}

		const auto nearest_id = find_nearest(key);
		auto& nearest_node = nodes[nearest_id];

		if (equal(key, nearest_node.key())) { //nearest has the same key
			return {make_iterator(nearest_id), false};
		}
		else { //nearest will be the parent of the node we're adding
			const auto insert_id = pop_free();
			auto& insert_node = nodes[insert_id];

			insert_node.parent = nearest_id;
			insert_node.key() = key;
	
			if (less(key, nearest_node.key())) {
				nearest_node.left = insert_id;
			}
			else {
				nearest_node.right = insert_id;
			}

			return {make_iterator(insert_id), true};
		}
	}

	/// Move a node, optionally replacing the desination's left or right subtree.
	void move_node(node_id from, node_id to, bool move_left_subtree, bool move_right_subtree) {
		auto& from_node = nodes[from];
		auto& to_node   = nodes[to];

		auto& from_parent = nodes[from_node.parent];
		if (from_parent.left == from) {
			from_parent.left = invalid_node;
		}
		else {
			from_parent.right = invalid_node;
		}

		from_node.parent = to_node.parent;

		if (!move_left_subtree) {
			from_node.left = to_node.left;
			if (!is_invalid_node(to_node.left)) {
				nodes[to_node.left].parent = from;
			}
		}
		if (!move_right_subtree) {
			from_node.right = to_node.right;
			if (!is_invalid_node(to_node.right)) {
				nodes[to_node.right].parent = from;
			}
		}

		if (to != root) {
			auto& to_parent = nodes[to_node.parent];
			if (to_parent.left == to) {
				to_parent.left = from;
			}
			else {
				to_parent.right = from;
			}
		}
		else {
			root = from;
		}

		push_free(to);
	}

	node_id find_exact(const key_type& key) const {
		#pragma HLS inline

		node_id current = root;
		node_id next = current;
		while (!is_invalid_node(next) && !equal(key, nodes[current].key())) {
			current = next;
			next = less(key, nodes[next].key()) ? nodes[next].left : nodes[next].right;
		}
		return equal(key, nodes[current].key()) ? current : static_cast<node_id>(invalid_node);
	}

	// Returns either the node with the given key, or if it doesn't exist, the node that would
	// be the parent of the node with the given key.
	node_id find_nearest(const key_type& key) const {
		#pragma HLS inline

		node_id current = root;
		node_id next = current;
		while (!is_invalid_node(next) && !equal(key, nodes[current].key())) {
			current = next;
			next = less(key, nodes[next].key()) ? nodes[next].left : nodes[next].right;
		}
		return current;
	}

	node_id find_min(node_id node) const {
		#pragma HLS inline

		if (is_invalid_node(node)) return invalid_node;

		node_id current = node;
		node_id next = nodes[node].left;
		while (!is_invalid_node(next)) {
			current = next;
			next = nodes[next].left;
		}
		return current;
	}

	node_id find_max(node_id node) const {
		#pragma HLS inline

		if (is_invalid_node(node)) return invalid_node;

		node_id current = node;
		node_id next = nodes[node].right;
		while (!is_invalid_node(next)) {
			current = next;
			next = nodes[next].right;
		}
		return current;
	}

	bool is_invalid_node(node_id node) const {
		#pragma HLS inline
		return (node >= invalid_node) || (!nodes[node].valid);
	}

	node_id pop_free() {
		#pragma HLS inline
		assert(free_count > 0);

		--free_count;
		const auto node_idx = free_nodes[free_count];

		auto& node_ref = nodes[node_idx];
		node_ref.valid = true;
		//node_ref.parent = invalid_node;
		//node_ref.left   = invalid_node;
		//node_ref.right  = invalid_node;

		return node_idx;
	}

	void push_free(node_id node) {
		#pragma HLS inline
		assert(free_count < free_nodes.size());

		free_nodes[free_count] = node;
		++free_count;

		auto& node_ref = nodes[node];
		node_ref.valid  = false;
		node_ref.parent = invalid_node;
		node_ref.left   = invalid_node;
		node_ref.right  = invalid_node;
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


	node_id root = invalid_node;
	container_type nodes;

	size_type free_count;
	ap_array<node_id, NodeCount> free_nodes;
};
