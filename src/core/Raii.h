// Live Studio - RAII helper kecil untuk objek libobs.
#pragma once

#include <obs.h>
#include <utility>

namespace ls {

// Konstanta alignment libobs (nilai sama dengan OBS_ALIGN_* di obs-defs.h).
constexpr uint32_t kAlignCenter = 0;
constexpr uint32_t kAlignLeft = 1u << 0;
constexpr uint32_t kAlignRight = 1u << 1;
constexpr uint32_t kAlignTop = 1u << 2;
constexpr uint32_t kAlignBottom = 1u << 3;

// Memegang satu reference ke obs_source_t.
class SourceRef {
public:
	SourceRef() = default;
	// Mengambil alih kepemilikan reference yang sudah ada (mis. hasil obs_source_create).
	explicit SourceRef(obs_source_t *owned) : s_(owned) {}
	// Membuat reference baru dari pointer pinjaman.
	static SourceRef borrow(obs_source_t *borrowed)
	{
		return SourceRef(borrowed ? obs_source_get_ref(borrowed) : nullptr);
	}

	SourceRef(const SourceRef &o) : s_(o.s_ ? obs_source_get_ref(o.s_) : nullptr) {}
	SourceRef(SourceRef &&o) noexcept : s_(o.s_) { o.s_ = nullptr; }
	SourceRef &operator=(SourceRef o) noexcept
	{
		std::swap(s_, o.s_);
		return *this;
	}
	~SourceRef() { reset(); }

	void reset()
	{
		if (s_) {
			obs_source_release(s_);
			s_ = nullptr;
		}
	}
	obs_source_t *get() const { return s_; }
	explicit operator bool() const { return s_ != nullptr; }

private:
	obs_source_t *s_ = nullptr;
};

// Memegang obs_data_t (settings).
class Data {
public:
	Data() : d_(obs_data_create()) {}
	Data(const Data &) = delete;
	Data &operator=(const Data &) = delete;
	Data(Data &&o) noexcept : d_(o.d_) { o.d_ = nullptr; }
	Data &operator=(Data &&o) noexcept
	{
		std::swap(d_, o.d_);
		return *this;
	}
	~Data()
	{
		if (d_)
			obs_data_release(d_);
	}
	obs_data_t *get() const { return d_; }

private:
	obs_data_t *d_ = nullptr;
};

} // namespace ls
