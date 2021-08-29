/*
 * Copyright (C) 2019 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <atomic>
#include <utility>
#include <cassert>
#include <shared_mutex>
#include <unordered_map>

/// <summary>
/// A simple lock-free linear search table.
/// The key values "one" and "zero" hold a special meaning (see <see cref="no_value"/> and <see cref="update_value"/>), so do not use them.
/// </summary>
template <typename TKey, typename TValue, uint32_t MAX_ENTRIES>
class lockfree_table : lockfree_table<TKey, TValue *, MAX_ENTRIES>
{
public:
	~lockfree_table()
	{
		clear(); // Free all pointers
	}

	using lockfree_table<TKey, TValue *, MAX_ENTRIES>::no_value;
	using lockfree_table<TKey, TValue *, MAX_ENTRIES>::update_value;

	/// <summary>
	/// Gets the value associated with the specified <paramref name="key"/>.
	/// This is a weak look up and may fail if another thread is erasing a value at the same time.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A reference to the associated value.</returns>
	TValue &at(TKey key) const
	{
		TValue *const value = lockfree_table<TKey, TValue *, MAX_ENTRIES>::at(key);
		if (value != nullptr)
			return *value;

		assert(false);
		return default_value(); // Fall back if table is key does not exist
	}

	/// <summary>
	/// Adds the specified key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="args">The constructor arguments to use for creation.</param>
	/// <returns>A reference to the newly added value.</returns>
	template <typename... Args>
	TValue &emplace(TKey key, Args... args)
	{
		// Create a pointer to the new value using copy construction
		TValue *const new_value = new TValue(std::forward<Args>(args)...);
		if (lockfree_table<TKey, TValue *, MAX_ENTRIES>::emplace(key, new_value))
			return *new_value;
		delete new_value;

		assert(false);
		return default_value(); // Fall back if table is full
	}

	/// <summary>
	/// Removes the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key)
	{
		TValue *const old_value = lockfree_table<TKey, TValue *, MAX_ENTRIES>::erase(key);
		if (old_value != nullptr)
		{
			delete old_value;
			return true;
		}
		return false;
	}
	/// <summary>
	/// Removes and returns the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <param name="value">The value associated with that key.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key, TValue &value)
	{
		TValue *const old_value = lockfree_table<TKey, TValue *, MAX_ENTRIES>::erase(key);
		if (old_value != nullptr)
		{
			// Move value to output argument and delete its pointer (which is no longer in use now)
			value = std::move(*old_value);
			delete old_value;
			return true;
		}
		return false;
	}

	/// <summary>
	/// Clears the entire table and deletes all keys.
	/// Note that another thread may add new values while this operation is in progress, so do not rely on it.
	/// </summary>
	void clear()
	{
		for (size_t i = 0; i < MAX_ENTRIES; ++i)
		{
			TValue *const old_value = _data[i].second;

			// Clear this entry so it can be used again
			if (TKey current_key = _data[i].first.exchange(no_value);
				current_key != no_value && current_key != update_value) // If this in update mode, we can assume the thread updating will reset the key to its intended value
			{
				// Delete any value attached to the entry, but only if there was one to begin with
				delete old_value;
			}
		}
	}

private:
	static inline TValue &default_value()
	{
		// Make default value thread local, so no data races occur after multiple threads failed to access a value
		static thread_local TValue _ = {}; return _;
	}
};

/// <summary>
/// Overload of the lock-free table for pointer value types, which avoids an extra indirection and stores the pointers directly.
/// </summary>
template <typename TKey, typename TValue, uint32_t MAX_ENTRIES>
class lockfree_table<TKey, TValue *, MAX_ENTRIES>
{
	using TValuePtr = TValue *;

public:
	~lockfree_table()
	{
		clear();
	}

	/// <summary>
	/// Special key indicating that the entry is empty.
	/// </summary>
	static constexpr TKey no_value = (TKey)0;
	/// <summary>
	/// Special key indicating that the entry is currently being updated.
	/// </summary>
	static constexpr TKey update_value = (TKey)1;

	/// <summary>
	/// Gets the pointer associated with the specified <paramref name="key"/>.
	/// This is a weak look up and may fail if another thread is erasing a value at the same time.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>The pointer associated with the key or <c>nullptr</c> if it was not found.</returns>
	TValuePtr at(TKey key) const
	{
		assert(key != no_value && key != update_value);

		// If the table is really big, reduce search time by using hash as start index
		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (_data[i].first.load(std::memory_order_acquire) == key)
			{
				// The pointer is guaranteed to be value at this point, or else key would have been in update mode
				return _data[i].second;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// Adds the specified key-pointer pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="value">The pointer to add.</param>
	/// <returns>The <c>true</c> if the key-pointer pair was added successfully or <c>false</c> if the table is full.</returns>
	bool emplace(TKey key, TValuePtr value)
	{
		assert(key != no_value && key != update_value);

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == no_value &&
				_data[i].first.compare_exchange_strong(test_key, update_value, std::memory_order_relaxed))
			{
				_data[i].second = value;

				_data[i].first.store(key, std::memory_order_release);

				return true;
			}
		}

		return false;
	}

	/// <summary>
	/// Removes and returns the pointer associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>The removed pointer if the key existed, <c>nullptr</c> otherwise.</returns>
	TValuePtr erase(TKey key)
	{
		if (key == no_value || key == update_value) // Cannot remove special keys
			return nullptr;

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			// Load and check before doing an expensive CAS
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == key)
			{
				// Get the value before freeing the entry up for other threads to fill again
				const TValuePtr old_value = _data[i].second;

				if (_data[i].first.compare_exchange_strong(test_key, no_value, std::memory_order_relaxed))
				{
					return old_value;
				}
			}
		}

		return nullptr;
	}

	/// <summary>
	/// Clears the entire table.
	/// </summary>
	void clear()
	{
		for (size_t i = 0; i < MAX_ENTRIES; ++i)
		{
			_data[i].first.exchange(no_value);
		}
	}

protected:
	std::pair<std::atomic<TKey>, TValuePtr> _data[MAX_ENTRIES];
};

/// <summary>
/// A simple thread-safe hash table, which splits locking across multiple buckets.
/// </summary>
template <typename TKey, typename TValue, uint32_t NUM_BUCKETS_LOG2 = 2, typename Hash = std::hash<TKey>>
class concurrent_hash_table
{
	static_assert(sizeof(TKey) <= sizeof(uint64_t));

	static constexpr uint32_t NUM_BUCKETS = (1 << NUM_BUCKETS_LOG2);

	static constexpr uint32_t calc_bucket_index(uint64_t key)
	{
		uint32_t hash = static_cast<uint32_t>(key >> 32) + static_cast<uint32_t>(key);
		hash ^= (hash >> NUM_BUCKETS_LOG2) ^ (hash >> (2 * NUM_BUCKETS_LOG2));
		hash &= (NUM_BUCKETS - 1);
		return hash;
	}

public:
	/// <summary>
	/// Gets a copy of the value associated with the specified <paramref name="key"/>.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A copy of the value.</returns>
	__forceinline TValue at(TKey key) const
	{
		const uint32_t bucket = calc_bucket_index((uint64_t)key);
		std::shared_lock<std::shared_mutex> lock(_mutex[bucket].object);
		return _data[bucket].at(key);
	}

	/// <summary>
	/// Adds the specified key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="args">The constructor arguments to use for creation.</param>
	/// <returns><c>true</c> if the key-value pair was added, <c>false</c> if the key already existed.</returns>
	template <typename... Args>
	bool emplace(TKey key, Args... args)
	{
		const uint32_t bucket = calc_bucket_index((uint64_t)key);
		std::unique_lock<std::shared_mutex> lock(_mutex[bucket].object);
		return _data[bucket].emplace(key, std::forward<Args>(args)...).second;
	}

	/// <summary>
	/// Removes the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key)
	{
		const uint32_t bucket = calc_bucket_index((uint64_t)key);
		std::unique_lock<std::shared_mutex> lock(_mutex[bucket].object);
		return _data[bucket].erase(key) != 0;
	}

	/// <summary>
	/// Removes all keys that are associated with the specified <paramref name="value"/> from the table.
	/// </summary>
	/// <param name="value">The value to compare with.</param>
	void erase_values(TValue value)
	{
		for (uint32_t bucket = 0; bucket < NUM_BUCKETS; ++bucket)
		{
			std::unique_lock<std::shared_mutex> lock(_mutex[bucket].object);
			for (auto it = _data[bucket].begin(); it != _data[bucket].end();)
				if (it->second == value)
					it = _data[bucket].erase(it);
				else
					++it;
		}
	}

	/// <summary>
	/// Clears the entire table.
	/// </summary>
	void clear()
	{
		for (uint32_t bucket = 0; bucket < NUM_BUCKETS; ++bucket)
		{
			const std::unique_lock<std::shared_mutex> lock(_mutex[bucket].object);
			_data[bucket].clear();
		}
	}

private:
	std::unordered_map<TKey, TValue, Hash> _data[NUM_BUCKETS];
	// Put each mutex on its own cache line to avoid false sharing
	mutable struct {
		alignas(std::hardware_destructive_interference_size) std::shared_mutex object;
	} _mutex[NUM_BUCKETS];
};
