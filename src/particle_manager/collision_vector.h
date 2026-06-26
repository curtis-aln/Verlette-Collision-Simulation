#pragma once

#include <vector>
#include <assert.h>
#include <cstdint>
#include <iterator>

/// @brief A collision pair returned by CollisionVector's iterator.
///
/// Unpacks the two 32-bit particle indices stored in a single packed uint64_t.
/// Returned by value from the iterator — no pointer into the buffer is exposed,
/// since the pair is unpacked on the fly.
struct CollisionPair
{
	int index_a; ///< Index of the first particle in the collision pair (always the lower index).
	int index_b; ///< Index of the second particle in the collision pair (always the higher index).
};

/// @brief A pre-allocated, fixed-capacity container for collision index pairs.
///
/// CollisionVector is designed for high-frequency, per-frame use in collision
/// detection pipelines. It avoids memory reallocation by reserving a fixed buffer
/// upfront and tracking active entries via a size counter. Clearing is O(1) —
/// no memory is freed or zeroed, the size is simply reset.
///
/// Pairs are stored packed as a single uint64_t (high 32 bits = index_a,
/// low 32 bits = index_b) for cache-friendly iteration and minimal memory use.
///
/// @note This container is NOT thread-safe. In a multithreaded collision pipeline,
///       each thread should own its own CollisionVector instance.
///
/// @note The container will assert in debug builds if you attempt to add beyond
///       the reserved capacity. Size your reserve accordingly at construction time.
///
/// ### Typical usage
/// @code
/// CollisionVector collisions(4096);
///
/// collisions.add(3, 17);
/// collisions.add(5, 42);
///
/// for (const CollisionPair& pair : collisions)
///     resolve(pair.index_a, pair.index_b);
///
/// collisions.clear(); // reset for next frame, no allocation
/// @endcode
struct alignas(64) CollisionVector
{
	// -------------------------------------------------------------------------
	// Iterator
	// -------------------------------------------------------------------------

	/// @brief A forward iterator over CollisionVector that yields CollisionPair values.
	///
	/// Unpacks each packed uint64_t on dereference. Satisfies std::forward_iterator,
	/// enabling range-for loops and standard library algorithms (std::for_each,
	/// std::count_if, etc.).
	///
	/// @note The iterator yields CollisionPair by value, not by reference, since
	///       the pair is unpacked on the fly from the packed buffer.
	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag;
		using value_type = CollisionPair;
		using difference_type = std::ptrdiff_t;
		using pointer = const CollisionPair*;
		using reference = CollisionPair; ///< by value — unpacked on dereference

		/// @brief Constructs an iterator pointing at the given packed pair entry.
		explicit Iterator(const uint64_t* ptr) : ptr_(ptr) {}

		/// @brief Dereferences the iterator, unpacking the current pair.
		/// @return A CollisionPair containing the two unpacked particle indices.
		CollisionPair operator*() const
		{
			return {
				static_cast<int>(*ptr_ >> 32),
				static_cast<int>(*ptr_ & 0xFFFFFFFF)
			};
		}

		/// @brief Advances the iterator to the next pair (pre-increment).
		Iterator& operator++()
		{
			++ptr_;
			return *this;
		}

		/// @brief Advances the iterator to the next pair (post-increment).
		Iterator operator++(int)
		{
			Iterator tmp = *this;
			++ptr_;
			return tmp;
		}

		/// @brief Returns true if two iterators point to the same entry.
		bool operator==(const Iterator& other) const { return ptr_ == other.ptr_; }

		/// @brief Returns true if two iterators point to different entries.
		bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }

	private:
		const uint64_t* ptr_; ///< Raw pointer into the packed collision pair buffer.
	};

	// -------------------------------------------------------------------------
	// Construction
	// -------------------------------------------------------------------------

	/// @brief Constructs a CollisionVector with a fixed-capacity buffer.
	///
	/// The internal buffer is allocated once and never grown. Choose a reserve
	/// size that comfortably covers your worst-case frame. A typical starting
	/// point is (particle_count * average_neighbours) / thread_count.
	///
	/// @param reserve Number of collision pairs to pre-allocate storage for.
	explicit CollisionVector(int reserve)
	{
		collision_pairs_.resize(reserve);
	}

	// -------------------------------------------------------------------------
	// Mutation
	// -------------------------------------------------------------------------

	/// @brief Records a collision between two particles.
	///
	/// Packs the two indices into a single uint64_t and appends it to the active
	/// region of the buffer. The caller is responsible for ensuring index_a < index_b
	/// to avoid recording both (a, b) and (b, a) for the same collision — the
	/// collision detection loop should enforce this with a forward-pair guard.
	///
	/// @param index_a Index of the first particle. Should be the lower index.
	/// @param index_b Index of the second particle. Should be the higher index.
	///
	/// @note Asserts in debug builds if capacity is exceeded. In release builds,
	///       behaviour is undefined if you write past the reserved size.
	void add(int index_a, int index_b)
	{
		assert(size_ < static_cast<int>(collision_pairs_.size())
			&& "CollisionVector capacity exceeded — increase reserve size.");
		assert(index_a < index_b
			&& "CollisionVector expects index_a < index_b to avoid duplicate pairs.");

		collision_pairs_[size_++] = (static_cast<uint64_t>(index_a) << 32)
			| static_cast<uint32_t>(index_b);
	}

	/// @brief Resets the active pair count to zero.
	///
	/// O(1) — no memory is freed or zeroed. Existing data in the buffer remains
	/// but will be overwritten on the next add(). Call this once per frame after
	/// all pairs have been consumed.
	void clear() { size_ = 0; }

	// -------------------------------------------------------------------------
	// Query
	// -------------------------------------------------------------------------

	/// @brief Returns the number of active collision pairs this frame.
	[[nodiscard]] int  size()  const { return size_; }

	/// @brief Returns true if no collision pairs have been recorded this frame.
	[[nodiscard]] bool empty() const { return size_ == 0; }

	/// @brief Returns the maximum number of pairs this instance can hold.
	[[nodiscard]] int  capacity() const { return static_cast<int>(collision_pairs_.size()); }

	/// @brief Returns the pair at the given index without bounds checking.
	/// @param index Must be in [0, size()).
	[[nodiscard]] CollisionPair operator[](int index) const
	{
		assert(index >= 0 && index < size_ && "CollisionVector index out of range.");
		const uint64_t packed = collision_pairs_[index];
		return { static_cast<int>(packed >> 32), static_cast<int>(packed & 0xFFFFFFFF) };
	}

	// -------------------------------------------------------------------------
	// Iteration
	// -------------------------------------------------------------------------

	/// @brief Returns an iterator to the first active collision pair.
	Iterator begin() const { return Iterator(collision_pairs_.data()); }

	/// @brief Returns a past-the-end iterator for the active region.
	Iterator end()   const { return Iterator(collision_pairs_.data() + size_); }

	// -------------------------------------------------------------------------
	// Data
	// -------------------------------------------------------------------------

private:
	std::vector<uint64_t> collision_pairs_{}; ///< Packed pair buffer. Each entry stores (index_a << 32 | index_b).
	int size_ = 0;                            ///< Number of active pairs in the buffer this frame.
};