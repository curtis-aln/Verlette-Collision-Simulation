#pragma once
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
//  Lock-free triple buffer (single producer, single consumer).
//
//  Three slots, each owned by exactly one party at any moment:
//    m_write  — owned by the update thread (only it touches this index)
//    m_read   — owned by the render thread (only it touches this index)
//    m_middle — the hand-off slot; low 2 bits = slot index, bit 2 = "dirty"
//               (holds a frame the renderer has not picked up yet)
//
//  Both sides swap with the middle slot using one atomic exchange, so there is
//  no window in which both threads can believe they own the same slot.
//
//  Requirements on T: default constructible, move assignable.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
class TripleBuffer
{
public:
	explicit TripleBuffer(int cell_render_reserve)
	{
		// Slots are already default-constructed by the array member.
		// Assign (not placement-new) so each object is constructed and
		// destroyed exactly once.
		for (T& b : m_buffers)
			b = T(cell_render_reserve);
	}

	// ── Update thread ─────────────────────────────────────────────────────

	// Buffer to write the next frame into.
	T& get_write_buffer()
	{
		return m_buffers[m_write];
	}

	// Hand the finished frame to the renderer and take back whichever slot
	// was sitting in the middle as the new write buffer.
	void publish()
	{
		m_write = m_middle.exchange(m_write | kDirty, std::memory_order_acq_rel)
			& kIndexMask;
		m_published.store(true, std::memory_order_release);
	}

	// ── Render thread ─────────────────────────────────────────────────────

	// True if the update thread has published a frame we haven't read yet.
	bool has_new_frame() const
	{
		return (m_middle.load(std::memory_order_acquire) & kDirty) != 0;
	}

	// True once the first frame has been published. Check this before
	// begin_read() so you never render a default-constructed buffer.
	bool has_published() const
	{
		return m_published.load(std::memory_order_acquire);
	}

	// Swap to the newest frame if there is one; otherwise keep the old one.
	const T& begin_read()
	{
		if (m_middle.load(std::memory_order_acquire) & kDirty)
		{
			m_read = m_middle.exchange(m_read, std::memory_order_acq_rel)
				& kIndexMask;
		}
		return m_buffers[m_read];
	}

	void end_read() {}

private:
	static constexpr int kDirty = 4;
	static constexpr int kIndexMask = 3;

	T                m_buffers[3];
	int              m_write = 0;            // update thread only
	int              m_read = 1;             // render thread only
	std::atomic<int> m_middle{ 2 };          // shared hand-off slot (clean)
	std::atomic<bool> m_published{ false };
};