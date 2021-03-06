﻿#pragma once
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#include "stdafx.h"
#else
#include <sys/sysinfo.h>
#endif

#ifndef MAX_VECTOR_SIZE
#ifdef DNN_SSE41
#define INSTRSET 5
#define MAX_VECTOR_SIZE 128
#endif // DNN_SSE41

#ifdef DNN_SSE42
#define INSTRSET 6
#define MAX_VECTOR_SIZE 128
#endif // DNN_SSE42

#ifdef DNN_AVX
#define INSTRSET 7
#define MAX_VECTOR_SIZE 256
#endif //DNN_AVX

#ifdef DNN_AVX2
#define INSTRSET 8
#define MAX_VECTOR_SIZE 256
#endif //DNN_AVX2

#ifdef DNN_AVX512
#define INSTRSET 9
#define MAX_VECTOR_SIZE 512
#endif //DNN_AVX512

#ifdef DNN_AVX512BW
#define INSTRSET 10
#define MAX_VECTOR_SIZE 512
#endif //DNN_AVX512BW
#endif // MAX_VECTOR_SIZE

#include "instrset.h"
#include "vectorclass.h"
#include "vectormath_common.h"
#include "vectormath_exp.h"
#include "vectormath_hyp.h"
#include "vectormath_trig.h"
#include "add-on/random/ranvec1.h"

#include <algorithm>
#include <array>
#include <atomic>
//#include <bit>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional> 
#include <future>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <clocale>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include "dnnl.hpp"
#include "dnnl_debug.h"
#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
#include "dnnl_ocl.hpp"
#elif DNNL_GPU_RUNTIME == DNNL_RUNTIME_SYCL
#include "dnnl_sycl.hpp"
#endif

#include "AlignedAllocator.h"
#include "ParallelFor.h"

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 255
#include "magic_enum.hpp"

namespace dnn
{
#ifdef _MSC_VER
	#define DNN_ALIGN(alignment) __declspec(align(alignment))
#else
	#define DNN_ALIGN(alignment) __attribute__((__aligned__(alignment)))
#endif
	#define DNN_SIMD_ALIGN DNN_ALIGN(64)
	
	typedef float Float;
	typedef size_t UInt;
	typedef unsigned char Byte;

#if defined(DNN_AVX512BW) || defined(DNN_AVX512)
	typedef Vec16f VecFloat;
	typedef Vec16fb VecFloatBool;
	constexpr auto VectorSize = 16ull;
	constexpr auto BlockedFmt = dnnl::memory::format_tag::nChw16c;
#elif defined(DNN_AVX2) || defined(DNN_AVX)
	typedef Vec8f VecFloat;
	typedef Vec8fb VecFloatBool;
	constexpr auto VectorSize = 8ull;
	constexpr auto BlockedFmt = dnnl::memory::format_tag::nChw8c;
#elif defined(DNN_SSE42) || defined(DNN_SSE41)
	typedef Vec4f VecFloat;
	typedef Vec4fb VecFloatBool;
	constexpr auto VectorSize = 4ull;
	constexpr auto BlockedFmt = dnnl::memory::format_tag::nChw4c;
#endif
	constexpr bool UseInplace = true;
	constexpr auto DivUp(const UInt& c) noexcept { return (((c - 1) / VectorSize) + 1) * VectorSize; }
	constexpr auto IsPlainDataFmt(const dnnl::memory::desc& md) noexcept { return md.data.format_kind == dnnl_blocked && md.data.format_desc.blocking.inner_nblks == 0; }
	constexpr auto IsBlockedDataFmt(const dnnl::memory::desc& md) noexcept { return md.data.format_kind == dnnl_blocked && md.data.format_desc.blocking.inner_nblks == 1 && md.data.format_desc.blocking.inner_idxs[0] == 1 && (md.data.format_desc.blocking.inner_blks[0] == 4 || md.data.format_desc.blocking.inner_blks[0] == 8 || md.data.format_desc.blocking.inner_blks[0] == 16); }
	constexpr auto PlainFmt = dnnl::memory::format_tag::nchw;
	constexpr auto GetDataFmt(const dnnl::memory::desc& md) noexcept
	{
		if (md.data.format_kind == dnnl_blocked)
		{
			if (md.data.format_desc.blocking.inner_nblks == 0)
				return PlainFmt;
			else
				if (md.data.format_desc.blocking.inner_nblks == 1 && md.data.format_desc.blocking.inner_idxs[0] == 1)
				{
					switch (md.data.format_desc.blocking.inner_blks[0])
					{
					case 4:
						return dnnl::memory::format_tag::nChw4c;
					case 8:
						return dnnl::memory::format_tag::nChw8c;
					case 16:
						return dnnl::memory::format_tag::nChw16c;
					default:
						return dnnl::memory::format_tag::undef;
					}
				}
		}

		return dnnl::memory::format_tag::undef;
	}

	template<typename T>
	static void InitArray(T* destination, const std::size_t elements, const int initValue = 0) noexcept
	{
		if (elements < 1048576ull)
			::memset(destination, initValue, elements * sizeof(T));
		else
		{
			const auto threads = elements < 2097152ull ? 2ull : elements < 8338608ull ? 4ull : 8ull;
			const auto part = elements / threads;
			for_i(threads, [=](const std::size_t thread) { ::memset(destination + part * thread, initValue, part * sizeof(T)); });
			if (elements % threads != 0)
				::memset(destination + part * threads, initValue, (elements - part * threads) * sizeof(T));
		}
	}

	struct aligned_free
	{
		void operator()(void* p)
		{
#if defined(_WIN32) || defined(__CYGWIN__)
			::_aligned_free(p);
#elif defined(__MINGW32__)
			::_mm_free(p);
#else
			::free(p);
#endif
		}
	};
	
	template<typename T>
	T* aligned_malloc(std::size_t size, std::size_t alignment) 
	{ 
#if defined(_WIN32) || defined(__CYGWIN__)
		return static_cast<T*>(::_aligned_malloc(size * sizeof(T), alignment));
#elif defined(__ANDROID__)
		return static_cast<T*>(::memalign(size * sizeof(T), alignment));
#elif defined(__MINGW32__)
		return  static_cast<T*>(::_mm_malloc(size * sizeof(T), alignment));
#else  // posix assumed
		return static_cast<T*>(::aligned_alloc(alignment, size * sizeof(T)));
#endif
	}

	template<class T> using unique_ptr_aligned = std::unique_ptr<T, aligned_free>;

	template<class T, std::size_t alignment> 
	unique_ptr_aligned<T> aligned_unique_ptr(std::size_t size, std::size_t align) { return unique_ptr_aligned<T>(static_cast<T*>(aligned_malloc<T>(size, align))); }

	template <typename T, std::size_t alignment> class AlignedArray
	{
		typedef typename std::size_t size_type;

	protected:
		unique_ptr_aligned<T> arrPtr = nullptr;
		T* dataPtr = nullptr;
		size_type nelems = 0;

	public:
		AlignedArray()
		{
			if (arrPtr)
				arrPtr.reset();

			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;
		}
		AlignedArray(const size_type elements, const T value = T()) 
		{
			if (arrPtr)
				arrPtr.reset();
				
			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;

			arrPtr = aligned_unique_ptr<T, alignment>(elements, alignment);
			if (arrPtr)
			{
				dataPtr = arrPtr.get();
				nelems = elements;

				if constexpr (std::is_floating_point_v<T>)
				{
					if constexpr (value == T(0))
						InitArray<T>(dataPtr, nelems, 0);
					else
						PRAGMA_OMP_SIMD()
						for (auto i = 0ull; i < nelems; i++)
							dataPtr[i] = value;
				}
				else
					for (auto i = 0ull; i < nelems; i++)
						dataPtr[i] = value;
			}
		}
		inline void release() noexcept
		{
			if (arrPtr)
				arrPtr.reset();

			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;
		}
		inline T* data() noexcept { return dataPtr; }
		inline const T* data() const noexcept { return dataPtr; }
		inline size_type size() const noexcept { return nelems; }
		inline void resize(size_type elements, const T value = T())
		{ 
			if (elements == nelems)
				return;

			if (arrPtr)
				arrPtr.reset();

			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;
			
			if (elements > 0)
			{
				arrPtr = aligned_unique_ptr<T, alignment>(elements, alignment);
				if (arrPtr)
				{
					dataPtr = arrPtr.get();
					nelems = elements;
					if constexpr (std::is_floating_point_v<T>)
					{
						if constexpr (value == T(0))
							InitArray<T>(dataPtr, nelems, 0);
						else
							PRAGMA_OMP_SIMD()
							for (auto i = 0ull; i < nelems; i++)
								dataPtr[i] = value;
					}
					else
						for (auto i = 0ull; i < nelems; i++)
							dataPtr[i] = value;
				}
			}		
		}
		inline T& operator[] (size_type i) noexcept { return dataPtr[i]; }
		inline const T& operator[] (size_type i) const noexcept { return dataPtr[i]; }
		inline bool empty() const noexcept { return nelems == 0; }
	};

	template <typename T> class AlignedMemory
	{
		typedef typename std::size_t size_type;

	protected:
		std::unique_ptr<dnnl::memory> arrPtr = nullptr;
		T* dataPtr = nullptr;
		size_type nelems = 0;
		
	public:
		AlignedMemory()
		{
			if (arrPtr)
				arrPtr.reset();

			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;
		}
		AlignedMemory(const dnnl::memory::desc& md, const dnnl::engine& engine, const T value = T())
		{
			if (md)
			{
				if (arrPtr)
					arrPtr.reset();

				nelems = 0;
				arrPtr = nullptr;
				dataPtr = nullptr;

				arrPtr = std::make_unique<dnnl::memory>(md, engine);
				if (arrPtr)
				{
					dataPtr = static_cast<T*>(arrPtr->get_data_handle());
					nelems = md.get_size() / sizeof(T);

					if constexpr (std::is_floating_point_v<T>)
					{
						if constexpr (value == T(0))
							InitArray<T>(dataPtr, nelems, 0);
						else
							PRAGMA_OMP_SIMD()
							for (auto i = 0ull; i < nelems; i++)
								dataPtr[i] = value;
					}
					else
						for (auto i = 0ull; i < nelems; i++)
							dataPtr[i] = value;
				}
			}
		}
		inline void release() noexcept
		{
			if (arrPtr)
				arrPtr.reset();

			nelems = 0;
			arrPtr = nullptr;
			dataPtr = nullptr;
		}
		inline T* data() noexcept { return dataPtr; }
		inline const T* data() const noexcept { return dataPtr; }
		inline size_type size() const noexcept { return nelems; }
		void resize(const dnnl::memory::desc& md, const dnnl::engine& engine, const T value = T())
		{
			if (md)
			{
				if (md.get_size() / sizeof(T) == nelems)
					return;

				if (arrPtr)
					arrPtr.reset();

				nelems = 0;
				arrPtr = nullptr;
				dataPtr = nullptr;

				if (md.get_size() / sizeof(T) > 0)
				{
					arrPtr = std::make_unique<dnnl::memory>(md, engine);
					if (arrPtr)
					{
						dataPtr = static_cast<T*>(arrPtr->get_data_handle());
						nelems = md.get_size() / sizeof(T);
						if constexpr (std::is_floating_point_v<T>)
						{
							if (value == T(0))
								InitArray<T>(dataPtr, nelems, 0);
							else
								PRAGMA_OMP_SIMD()
								for (auto i = 0ull; i < nelems; i++)
									dataPtr[i] = value;
						}
						else
							for (auto i = 0ull; i < nelems; i++)
								dataPtr[i] = value;
					}
				}
			}
		}
		void resize(const size_type n, const size_type c, const dnnl::memory::data_type dtype, const dnnl::memory::format_tag format, const dnnl::engine& engine, const T value = T())
		{
			resize(dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(n), dnnl::memory::dim(c) }), dtype, format), engine, value);
		}
		void resize(const size_type n, const size_type c, const size_type w, const dnnl::memory::data_type dtype, const dnnl::memory::format_tag format, const dnnl::engine& engine, const T value = T())
		{
			resize(dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(n), dnnl::memory::dim(c), dnnl::memory::dim(w) }), dtype, format), engine, value);
		}
		void resize(const size_type n, const size_type c, const size_type h, const size_type w, const dnnl::memory::data_type dtype, const dnnl::memory::format_tag format, const dnnl::engine& engine, const T value = T())
		{
			resize(dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(n), dnnl::memory::dim(c), dnnl::memory::dim(h), dnnl::memory::dim(w) }), dtype, format), engine, value);
		}
		void resize(const size_type n, const size_type c, const size_type d, const size_type h, const size_type w, const dnnl::memory::data_type dtype, const dnnl::memory::format_tag format, const dnnl::engine& engine, const T value = T())
		{
			resize(dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(n), dnnl::memory::dim(c), dnnl::memory::dim(d), dnnl::memory::dim(h), dnnl::memory::dim(w) }), dtype, format), engine, value);
		}
		inline T& operator[] (size_type i) noexcept { return dataPtr[i]; }
		inline const T& operator[] (size_type i) const noexcept { return dataPtr[i]; }
		inline bool empty() const noexcept { return nelems == 0; }
	};

	typedef AlignedMemory<Float> FloatArray;
	typedef AlignedArray<Byte, 64ull> ByteArray;
	typedef std::vector<Float, AlignedAllocator<Float, 64ull>> FloatVector;
	//constexpr bool IS_LITTLE_ENDIAN = std::endian::native == std::endian::little;
	constexpr auto NEURONS_LIMIT = Float(1000);   // limit for all the neurons and derivative [-NEURONS_LIMIT,NEURONS_LIMIT]
	constexpr auto WEIGHTS_LIMIT = Float(100);    // limit for all the weights and biases [-WEIGHTS_LIMIT,WEIGHTS_LIMIT]
	constexpr auto LIGHT_COMPUTE = 4ull;          // number of threads
	constexpr auto MEDIUM_COMPUTE = 8ull;
	constexpr auto FloatSquare(const Float& value) noexcept { return (value * value); }
	template<typename T>
	constexpr auto Clamp(const T& v, const T& lo, const T& hi) noexcept { return (v < lo) ? lo : (hi < v) ? hi : v; }
	template<typename T>
	constexpr auto Saturate(const T& value) noexcept { return (value > T(255)) ? Byte(255) : (value < T(0)) ? Byte(0) : Byte(value); }
	constexpr auto GetColorFromRange(const Float& range, const Float& minimum, const Float& value) noexcept { return Saturate<Float>(Float(255) - ((value - minimum) * range)); }
	constexpr auto GetColorRange(const Float& min, const Float& max) noexcept { return (min == max) ? Float(0) : Float(255) / ((std::signbit(min) && std::signbit(max)) ? -(min + max) : (max - min)); }

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
	static const auto nwl = std::string("\r\n");
#else // assuming Linux
	static const auto nwl = std::string("\n");
#endif
	static const auto tab = std::string("\t");
	static const auto dtab = std::string("\t\t");	
	
	static void SleepYield(std::chrono::microseconds us)
	{
		const auto start = std::chrono::high_resolution_clock::now();
		const auto end = start + us;
		do
		{
			std::this_thread::yield();
		}
		while (std::chrono::high_resolution_clock::now() < end);
	}

#ifdef DNN_FAST_SEED
	template<typename T>
	inline static T Seed() noexcept
	{
		return static_cast<T>(__rdtsc());
	}
#else
	static int GetPhysicalSeedType() noexcept
	{
		int abcd[4];						// return values from cpuid instruction
		
		cpuid(abcd, 7);						// call cpuid function 7
		if (abcd[1] & (1 << 18)) 
			return 3; // ebx bit 18: RDSEED available
		cpuid(abcd, 1);						// call cpuid function 1
		if (abcd[2] & (1 << 30)) 
			return 2; // ecx bit 30: RDRAND available
		if (abcd[3] & (1 << 4)) 
			return 1; // edx bit  4: RDTSC available

		return 0;
	}
	
	static int PhysicalSeedType = -1;
	template<typename T>
	T Seed() noexcept
	{
		if (PhysicalSeedType < 0)
			PhysicalSeedType = GetPhysicalSeedType();
		
		uint32_t ran = 0;					// random number
		switch (PhysicalSeedType) 
		{
		case 1:								// use RDTSC instruction
			ran = static_cast<uint32_t>(__rdtsc());
			break;
		case 2:								// use RDRAND instruction
			while (_rdrand32_step(&ran) == 0) {}
			break;
		case 3:								// use RDSEED instruction */
			while (_rdseed32_step(&ran) == 0) {}
			break;
		}
		
		return static_cast<T>(ran);			// return random number
	}
#endif

	inline static auto BernoulliVecFloat(const Float prob = Float(0.5)) noexcept
	{
		static thread_local auto generator = Ranvec1(3);
		generator.init(Seed<int>(), static_cast<int>(std::hash<std::thread::id>()(std::this_thread::get_id())));
#if defined(DNN_AVX512BW) || defined(DNN_AVX512)
		return select(generator.random16f() < prob, VecFloat(1), VecFloat(0));
#elif defined(DNN_AVX2) || defined(DNN_AVX)
		return select(generator.random8f() < prob, VecFloat(1), VecFloat(0));
#elif defined(DNN_SSE42) || defined(DNN_SSE41)
		return select(generator.random4f() < prob, VecFloat(1), VecFloat(0));
#endif
	}

	template<typename T>
	static auto Bernoulli(const Float prob = Float(0.5)) noexcept
	{
		static thread_local auto generator = std::mt19937(Seed<unsigned>());
		return static_cast<T>(std::bernoulli_distribution(double(prob))(generator));
	}

	template<typename T>
	static auto UniformInt(const T min, const T max) noexcept
	{
		static thread_local auto generator = std::mt19937(Seed<unsigned>());
		return std::uniform_int_distribution<T>(min, max)(generator);
	}

	template<typename T>
	static auto UniformReal(const T min, const T max) noexcept
	{
		static thread_local auto generator = std::mt19937(Seed<unsigned>());
		return std::uniform_real_distribution<T>(min, max)(generator);
	}
		
	static auto FloatToString(const Float value, const std::streamsize precision = 8)
	{
		std::stringstream stream; 
		stream << std::setprecision(precision) << value;
		return stream.str();
	}

	static auto FloatToStringFixed(const Float value, const std::streamsize precision = 8)
	{
		std::stringstream stream; 
		stream << std::setprecision(precision) << std::fixed << value;
		return stream.str();
	}

	static auto FloatToStringScientific(const Float value, const std::streamsize precision = 4)
	{
		std::stringstream stream; 
		stream << std::setprecision(precision) << std::scientific << value;
		return stream.str();
	}

   	static auto GetFileSize(std::string fileName)
	{
		auto file = std::ifstream(fileName, std::ifstream::in | std::ifstream::binary);

		if (!file.is_open() || file.bad())
			return std::streamsize(-1);

		file.seekg(0, std::ios::beg);
		const auto start = file.tellg();
		file.seekg(0, std::ios::end);
		const auto end = file.tellg();
		file.close();
		
		return static_cast<std::streamsize>(end - start);
	}

	static auto StringToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), ::tolower);
		return text;
	};

	static auto IsStringBool(std::string text)
	{
		const auto textLower = StringToLower(text);
		
		if (textLower == "true" || textLower == "yes" || textLower == "false" || textLower == "no")
			return true;

		return false;
	}

	static auto StringToBool(std::string text)
	{
		const auto textLower = StringToLower(text);
		
		if (textLower == "true" || textLower == "yes")
			return true;

		return false;
	}

	static auto GetTotalFreeMemory()
	{
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
		MEMORYSTATUSEX statusEx;
		statusEx.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&statusEx);
		std::cout << std::string("Available memory: ") << std::to_string(statusEx.ullAvailPhys/1024/1024) << std::string("/") << std::to_string(statusEx.ullTotalPhys/1024/1024) << " MB" << std::endl;
		return statusEx.ullAvailPhys;
#else        
		struct sysinfo info;
		if (sysinfo(&info) == 0)
		{
			std::cout << std::string("Available memory: ") << std::to_string(info.freeram*info.mem_unit/1024/1024) << std::string("/") << std::to_string(info.totalram*info.mem_unit/1024/1024) << " MB" << std::endl;
			return static_cast<UInt>(info.freeram * info.mem_unit);
		}
		else
			return static_cast<UInt>(0);
#endif
	}
	
	static auto CaseInsensitiveReplace(std::string::const_iterator begin, std::string::const_iterator end, const std::string& before, const std::string& after)
	{
		auto retval = std::string("");
		auto dest = std::back_insert_iterator<std::string>(retval);
		auto current = begin;
		auto next = std::search(current, end, before.begin(), before.end(), [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });

		while (next != end)
		{
			std::copy(current, next, dest);
			std::copy(after.begin(), after.end(), dest);
			current = next + before.size();
			next = std::search(current, end, before.begin(), before.end(), [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
		}

		std::copy(current, next, dest);

		return retval;
	}

	// From Stack Overflow https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
	static auto Trim(std::string text)
	{
		text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](int ch) { return !std::isspace(ch); }));
		text.erase(std::find_if(text.rbegin(), text.rend(), [](int ch) { return !std::isspace(ch); }).base(), text.end());
		return text;
	}

	// from Stack Overflow https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
	static auto& SafeGetline(std::istream& is, std::string& line)
	{
		line.clear();

		// The characters in the stream are read one-by-one using a std::streambuf.
		// That is faster than reading them one-by-one using the std::istream.
		// Code that uses streambuf this way must be guarded by a sentry object.
		// The sentry object performs various tasks,
		// such as thread synchronization and updating the stream state.

		auto se = std::istream::sentry(is, true);
		auto sb = is.rdbuf();

		for (;;) 
		{
			auto c = sb->sbumpc();
			switch (c) 
			{
			case '\n':
				return is;
			case '\r':
				if (sb->sgetc() == '\n')
					sb->sbumpc();
				return is;
			case std::streambuf::traits_type::eof():
				// Also handle the case when the last line has no line ending
				if (line.empty())
					is.setstate(std::ios::eofbit);
				return is;
			default:
				line += static_cast<char>(c);
			}
		}
	}

	template <typename T>
	constexpr void SwapEndian(T& buffer)
	{
		static_assert(std::is_standard_layout<T>::value, "SwapEndian supports standard layout types only");
		auto startIndex = static_cast<char*>((void*)buffer.data());
		auto endIndex = startIndex + sizeof(buffer);
		std::reverse(startIndex, endIndex);
	}

	// from oneDNN sample code
	// Read from memory, write to handle
	inline void read_from_dnnl_memory(void* handle, dnnl::memory& mem) {
		dnnl::engine eng = mem.get_engine();
		auto size = mem.get_desc().get_size();

		if (!handle) throw std::runtime_error("handle is nullptr.");

#ifdef DNNL_WITH_SYCL
		bool is_cpu_sycl = (DNNL_CPU_RUNTIME == DNNL_RUNTIME_SYCL
			&& eng.get_kind() == dnnl::engine::kind::cpu);
		bool is_gpu_sycl = (DNNL_GPU_RUNTIME == DNNL_RUNTIME_SYCL
			&& eng.get_kind() == dnnl::engine::kind::gpu);
		if (is_cpu_sycl || is_gpu_sycl) {
			auto mkind = dnnl::sycl_interop::get_memory_kind(mem);
			if (mkind == dnnl::sycl_interop::memory_kind::buffer) {
				auto buffer = dnnl::sycl_interop::get_buffer<uint8_t>(mem);
				auto src = buffer.get_access<cl::sycl::access::mode::read>();
				uint8_t* src_ptr = src.get_pointer();
				if (!src_ptr)
					throw std::runtime_error("get_pointer returned nullptr.");
				for (auto i = 0ull; i < size; ++i)
					((uint8_t*)handle)[i] = src_ptr[i];
			}
			else {
				assert(mkind == dnnl::sycl_interop::memory_kind::usm);
				uint8_t* src_ptr = (uint8_t*)mem.get_data_handle();
				if (!src_ptr)
					throw std::runtime_error("get_data_handle returned nullptr.");
				if (is_cpu_sycl) {
					for (auto i = 0ull; i < size; ++i)
						((uint8_t*)handle)[i] = src_ptr[i];
				}
				else {
					auto sycl_queue
						= dnnl::sycl_interop::get_queue(dnnl::stream(eng));
					sycl_queue.memcpy(handle, src_ptr, size).wait();
				}
			}
			return;
		}
#endif
#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
		if (eng.get_kind() == dnnl::engine::kind::gpu) {
			dnnl::stream s(eng);
			cl_command_queue q = dnnl::ocl_interop::get_command_queue(s);
			cl_mem m = dnnl::ocl_interop::get_mem_object(mem);

			cl_int ret = clEnqueueReadBuffer(
				q, m, CL_TRUE, 0, size, handle, 0, NULL, NULL);
			if (ret != CL_SUCCESS)
				throw std::runtime_error("clEnqueueReadBuffer failed.");
			return;
		}
#endif

		if (eng.get_kind() == dnnl::engine::kind::cpu) {
			uint8_t* src = static_cast<uint8_t*>(mem.get_data_handle());
			if (!src) throw std::runtime_error("get_data_handle returned nullptr.");
			for (auto i = 0ull; i < size; ++i)
				((uint8_t*)handle)[i] = src[i];
			return;
		}

		assert(!"not expected");
	}

	// Read from handle, write to memory
	inline void write_to_dnnl_memory(void* handle, dnnl::memory& mem) {
		dnnl::engine eng = mem.get_engine();
		auto size = mem.get_desc().get_size();

		if (!handle) throw std::runtime_error("handle is nullptr.");

#ifdef DNNL_WITH_SYCL
		bool is_cpu_sycl = (DNNL_CPU_RUNTIME == DNNL_RUNTIME_SYCL
			&& eng.get_kind() == dnnl::engine::kind::cpu);
		bool is_gpu_sycl = (DNNL_GPU_RUNTIME == DNNL_RUNTIME_SYCL
			&& eng.get_kind() == dnnl::engine::kind::gpu);
		if (is_cpu_sycl || is_gpu_sycl) {
			auto mkind = dnnl::sycl_interop::get_memory_kind(mem);
			if (mkind == dnnl::sycl_interop::memory_kind::buffer) {
				auto buffer = dnnl::sycl_interop::get_buffer<uint8_t>(mem);
				auto dst = buffer.get_access<cl::sycl::access::mode::write>();
				uint8_t* dst_ptr = dst.get_pointer();
				if (!dst_ptr)
					throw std::runtime_error("get_pointer returned nullptr.");
				for (auto i = 0ull; i < size; ++i)
					dst_ptr[i] = ((uint8_t*)handle)[i];
			}
			else {
				assert(mkind == dnnl::sycl_interop::memory_kind::usm);
				uint8_t* dst_ptr = (uint8_t*)mem.get_data_handle();
				if (!dst_ptr)
					throw std::runtime_error("get_data_handle returned nullptr.");
				if (is_cpu_sycl) {
					for (auto i = 0ull; i < size; ++i)
						dst_ptr[i] = ((uint8_t*)handle)[i];
				}
				else {
					auto sycl_queue
						= dnnl::sycl_interop::get_queue(dnnl::stream(eng));
					sycl_queue.memcpy(dst_ptr, handle, size).wait();
				}
			}
			return;
		}
#endif
#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
		if (eng.get_kind() == dnnl::engine::kind::gpu) {
			dnnl::stream s(eng);
			cl_command_queue q = dnnl::ocl_interop::get_command_queue(s);
			cl_mem m = dnnl::ocl_interop::get_mem_object(mem);

			cl_int ret = clEnqueueWriteBuffer(
				q, m, CL_TRUE, 0, size, handle, 0, NULL, NULL);
			if (ret != CL_SUCCESS)
				throw std::runtime_error("clEnqueueWriteBuffer failed.");
			return;
		}
#endif

		if (eng.get_kind() == dnnl::engine::kind::cpu) {
			uint8_t* dst = static_cast<uint8_t*>(mem.get_data_handle());
			if (!dst) throw std::runtime_error("get_data_handle returned nullptr.");
			for (auto i = 0ull; i < size; ++i)
				dst[i] = ((uint8_t*)handle)[i];
			return;
		}

		assert(!"not expected");
	}
}