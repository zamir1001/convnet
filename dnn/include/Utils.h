﻿#pragma once
#if defined _WIN32 || defined __CYGWIN__ || defined __MINGW32__
#include "stdafx.h"
#else
#include <sys/sysinfo.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
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

#ifndef MAX_VECTOR_SIZE
#ifdef DNN_AVX512
#define INSTRSET 9
#define MAX_VECTOR_SIZE 512
#else
#define INSTRSET 8
#define MAX_VECTOR_SIZE 256
#endif
#endif

#include "vectorclass.h"
#include "vectormath_hyp.h"
#include "vectormath_exp.h"
#include "vectormath_trig.h"
#include "add-on/random/ranvec1.h"

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 255
#include "include/magic_enum.hpp"

#include "dnnl.hpp"

#include "AlignedAllocator.h"
#include "ParallelFor.h"


namespace dnn
{
#if defined(_MSC_VER)
#define DNN_ALIGN(alignment) __declspec(align(alignment))
#else
#define DNN_ALIGN(alignment) __attribute__((__aligned__(alignment)))
#endif
#define DNN_SIMD_ALIGN DNN_ALIGN(64)
#define DNN_UNREF_PAR(P) (P)

	typedef float Float;
	typedef unsigned char Byte;
	typedef std::vector<Float, AlignedAllocator<Float, 64ull>> FloatVector;
	typedef std::vector<Byte, AlignedAllocator<Byte, 64ull>> ByteVector;
	
	constexpr auto LIGHT_COMPUTE = 4ull;         // number of threads
	constexpr auto MEDIUM_COMPUTE = 8ull;
	constexpr auto FloatSquare(const Float& value) noexcept { return (value * value); }
	template<typename T>
	constexpr auto Clamp(const T& v, const T& lo, const T& hi) noexcept { return (v < lo) ? lo : (hi < v) ? hi : v; }
	template<typename T>
	constexpr auto Saturate(const T& value) noexcept { return (value > T(255)) ? Byte(255) : (value < T(0)) ? Byte(0) : Byte(value); }
	constexpr auto GetColorFromRange(const Float& range, const Float& minimum, const Float& value) noexcept { return Saturate<Float>(Float(255) - ((value - minimum) * range)); }
	constexpr auto GetColorRange(const Float& min, const Float& max) noexcept { return (min == max) ? Float(0) : Float(255) / ((std::signbit(min) && std::signbit(max)) ? -(min + max) : (max - min)); }
	
#if defined _WIN32 || defined __CYGWIN__ || defined __MINGW32__
	static const auto nwl = std::string("\r\n");
#else // assuming Linux
	static const auto nwl = std::string("\n");
#endif
	static const auto tab = std::string("\t");
	static const auto dtab = std::string("\t\t");

	constexpr auto PlainFmt = dnnl::memory::format_tag::nchw;
#ifdef DNN_AVX512
	typedef Vec16f VecFloat;
	constexpr auto VectorSize = 16ull;
	constexpr auto BlockedFmt = dnnl::memory::format_tag::nChw16c;
#else // assuming AVX2
	typedef Vec8f VecFloat;
	constexpr auto VectorSize = 8ull;
	constexpr auto BlockedFmt = dnnl::memory::format_tag::nChw8c;
#endif

	constexpr auto DivUp(const size_t& c) noexcept { return (((c - 1) / VectorSize) + 1) * VectorSize; }
	constexpr auto IsPlainDataFmt(const dnnl::memory::desc& md) noexcept { return md.data.format_kind == dnnl_blocked && md.data.format_desc.blocking.inner_nblks == 0; }
	constexpr auto IsBlockedDataFmt(const dnnl::memory::desc& md) noexcept { return md.data.format_kind == dnnl_blocked && md.data.format_desc.blocking.inner_nblks == 1 && md.data.format_desc.blocking.inner_idxs[0] == 1 && md.data.format_desc.blocking.inner_blks[0] == VectorSize; }
	constexpr auto GetDataFmt(const dnnl::memory::desc& md) noexcept
	{
		if (md.data.format_kind == dnnl_blocked)
		{
			if (md.data.format_desc.blocking.inner_nblks == 0)
				return PlainFmt;
			else 
				if (md.data.format_desc.blocking.inner_nblks == 1 && md.data.format_desc.blocking.inner_idxs[0] == 1)
				{
					if (md.data.format_desc.blocking.inner_blks[0] == VectorSize)
						return BlockedFmt;
					else if (md.data.format_desc.blocking.inner_blks[0] == 4)
						return dnnl::memory::format_tag::nChw4c;
				}
		}

		return dnnl::memory::format_tag::undef;
	}

	inline static void ZeroFloatVector(Float* destination, const size_t elements) noexcept
	{
		if (elements < 1048576ull)
			std::memset(destination, 0, elements * sizeof(Float));
		else
		{
			const auto threads = elements < 2097152ull ? 2ull : elements < 8338608ull ? LIGHT_COMPUTE : MEDIUM_COMPUTE;
			const auto part = elements / threads;
			for_i(threads, [=](const size_t thread) { std::memset(destination + part * thread, 0, part * sizeof(Float)); });
			if (elements % threads != 0)
				std::memset(destination + part * threads, 0, (elements - part * threads) * sizeof(Float));
		}
	}

	inline static void ZeroFloatVectorAllocate(FloatVector& destination, const size_t elements) noexcept
	{
		if (destination.size() < elements)
			destination = FloatVector(elements);
			
		ZeroFloatVector(destination.data(), elements);
	}
	
	inline static const auto BernoulliVecFloat(const Float prob = Float(0.5)) noexcept
	{
		static thread_local auto generator = Ranvec1(3);
		generator.init(static_cast<int>(__rdtsc()), static_cast<int>(std::hash<std::thread::id>()(std::this_thread::get_id())));
#ifdef DNN_AVX512
		return select(generator.random16f() < prob, VecFloat(1), VecFloat(0));
#else
		return select(generator.random8f() < prob, VecFloat(1), VecFloat(0));
#endif
	}

	template<typename T>
	static const auto Bernoulli(const Float prob = Float(0.5)) noexcept
	{
		static thread_local auto generator = std::mt19937(static_cast<unsigned>(__rdtsc()));
		return static_cast<T>(std::bernoulli_distribution(double(prob))(generator));
	}

	template<typename T>
	static const auto UniformInt(const T min, const T max) noexcept
	{
		static thread_local auto generator = std::mt19937(static_cast<unsigned>(__rdtsc()));
		return std::uniform_int_distribution<T>(min, max)(generator);
	}

	template<typename T>
	static const auto UniformReal(const T min, const T max) noexcept
	{
		static thread_local auto generator = std::mt19937(static_cast<unsigned>(__rdtsc()));
		return std::uniform_real_distribution<T>(min, max)(generator);
	}
		
	static auto FloatToString(const Float value, const std::streamsize precision = 8)
	{
		return (std::ostringstream() << std::setprecision(precision) << value).str();
	}

	static auto FloatToStringFixed(const Float value, const std::streamsize precision = 8)
	{
		return (std::ostringstream() << std::setprecision(precision) << std::fixed << value).str();
	}

	static auto FloatToStringScientific(const Float value, const std::streamsize precision = 4)
	{
		return (std::ostringstream() << std::setprecision(precision) << std::scientific << value).str();
	}

   	static const auto GetFileSize(const char* fileName)
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

	static const auto IsStringBool(std::string text)
	{
		auto str = std::string(text);
		std::transform(str.begin(), str.end(), str.begin(), std::tolower);

		if (str == "true" || str == "yes" || str =="false" || str == "no")
			return true;

		return false;
	}

	static const auto StringToBool(std::string text)
	{
		auto str = std::string(text);
		std::transform(str.begin(), str.end(), str.begin(), std::tolower);

		if (str == "true" || str == "yes")
			return true;

		return false;
	}

	static const auto GetTotalFreeMemory()
	{
#if defined _WIN32 || defined __CYGWIN__ || defined __MINGW32__
		MEMORYSTATUSEX statusEx;
		statusEx.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&statusEx);
		return statusEx.ullAvailPhys;
#else
		struct sysinfo info;
		if (sysinfo(&info) == 0)
			return static_cast<size_t>(info.totalram - info.freeram);
		else
			return 0ull;
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
	static auto Trim(std::string s)
	{
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
		return s;
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
		static_assert(std::is_standard_layout<T>::value, "SwapEndian support standard layout types only");
		auto startIndex = static_cast<char*>((void*)buffer.data());
		auto endIndex = startIndex + sizeof(buffer);
		std::reverse(startIndex, endIndex);
	}
}