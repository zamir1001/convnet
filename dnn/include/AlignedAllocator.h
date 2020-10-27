#pragma once
#include <stdlib.h>
#include <string>
#include <utility>

#ifdef __MINGW32__
#include <mm_malloc.h>
#endif

namespace dnn
{
	template <typename T, std::size_t alignment>
	class AlignedAllocator
	{
	public:
		typedef T value_type;
		typedef T* pointer;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;
		typedef T& reference;
		typedef const T& const_reference;
		typedef const T* const_pointer;

		template <typename U>
		struct rebind
		{
			typedef AlignedAllocator<U, alignment> other;
		};

		AlignedAllocator() {}

		template <typename U>
		AlignedAllocator(const AlignedAllocator<U, alignment>&) {}

		const_pointer address(const_reference value) const { return std::addressof(value); }

		pointer address(reference value) const { return std::addressof(value); }

		pointer allocate(const size_type size, const void* = nullptr)
		{
			void* p = AlignedAlloc(alignment, sizeof(T) * size);

			if (!p && size > 0ull)
				throw std::exception("failed to allocate");

			return static_cast<pointer>(p);
		}

		size_type max_size() const { return ~static_cast<std::size_t>(0ull) / sizeof(T); }

		void deallocate(pointer ptr, size_type) { AlignedFree(ptr); }

		template <class U, class V>
		void construct(U* ptr, const V& value)
		{
			void* p = ptr;
			::new (p) U(value);
		}

		template <class U, class... Args>
		void construct(U* ptr, Args &&... args)
		{
			void* p = ptr;
			::new (p) U(std::forward<Args>(args)...);
		}

		template <class U>
		void construct(U* ptr)
		{
			void* p = ptr;
			::new (p) U();
		}

		template <class U>
		void destroy(U* ptr)
		{
			ptr->~U();
		}

	private:
		void* AlignedAlloc(const size_type align, const size_type size) const
		{
#if defined(_WIN32) || defined(__CYGWIN__)
			return ::_aligned_malloc(size, align);
#elif defined(__ANDROID__)
			return ::memalign(align, size);
#elif defined(__MINGW32__)
			return ::_mm_malloc(size, align);
#else  // posix assumed
			void* p;
			if (::posix_memalign(&p, align, size) != 0)
				p = 0;
			return p;
#endif
		}

		void AlignedFree(pointer ptr)
		{
#if defined(_WIN32) || defined(__CYGWIN__)
			::_aligned_free(ptr);
#elif defined(__MINGW32__)
			::_mm_free(ptr);
#else
			::free(ptr);
#endif
		}
	};

	template <typename T1, typename T2, std::size_t alignment>
	inline bool operator==(const AlignedAllocator<T1,alignment>&, const AlignedAllocator<T2,alignment>&) { return true; }

	template <typename T1, typename T2, std::size_t alignment>
	inline bool operator!=(const AlignedAllocator<T1,alignment>&, const AlignedAllocator<T2,alignment>&) { return false; }
}