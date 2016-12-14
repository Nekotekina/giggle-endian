/*
endian.hpp: Simple header-only endianness support library for C++
Copyright (C) 2016 Ivan G. / nekotekina@gmail.com
This file may be modified and distributed under the terms of the MIT license:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// This header provides following template aliases: `le`, `be`, `ne`, `re`
// stx::le<T,A> -- Little Endian
// stx::be<T,A> -- Big Endian
// stx::ne<T,A> -- Normal Endian (implementation-defined, it shall be either LE or BE)
// stx::re<T,A> -- Reversed Endian (implementation-defined, shall not be equal to ne<>)
// First template argument is the underlying type (hopefully may be any type).
// Second optional template argument is explicit alignment (native by default).
// Alignment may be increased and decreased.
// Setting greater alignment works similar to alignas() and isn't very useful.
// Setting small alignment (especially 1) is the alternative to `#pragma pack`.
// Struct has public method get() and public member `data` (implementation-defined).

// stx::be<std::uint32_t> -- big endian uint32_t
// stx::le<std::uint32_t> -- little endian uint32_t
// stx::be<std::uint32_t, 2> -- big endian uint32_t with enforced alignment 2 (packing)
// stx::ne<std::uint32_t> -- use native endianness (either LE or BE, implementation-defined)
// stx::re<std::uint32_t> -- use foreign endianness (either BE or LE, NOT equal to ne<>)

#pragma once

#include <type_traits>
#include <cstdint>

namespace stx
{
	// Class proposed in https://howardhinnant.github.io/endian.html
	enum class endian
	{
		little,
		big,
// Detection from http://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
		native = big,
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || defined(_M_ARM)
		native = little,
#else
#error "Unknown endianness"
#endif
	};

	namespace detail
	{
		using uchar = unsigned char;

		template <typename T, std::size_t Size, std::size_t Align>
		struct alignas(Align) endian_buffer
		{
			using type = endian_buffer;

			// Unoptimized generic copying with swapping bytes for unaligned data
			static inline void reverse(uchar* dst, const uchar* src)
			{
				for (std::size_t i = 0; i < Size; i++)
				{
					dst[i] = src[Size - 1 - i];
				}
			}

			// Unoptimized generic copying for unaligned data
			static inline void copy(uchar* dst, const uchar* src)
			{
				for (std::size_t i = 0; i < Size; i++)
				{
					dst[i] = src[i];
				}
			}

			static inline void put_re(type& dst, const T& src)
			{
				reverse(dst.data, reinterpret_cast<const uchar*>(&src));
			}

			static inline T get_re(const type& src)
			{
				T dst;
				reverse(reinterpret_cast<uchar*>(&dst), src.data);
				return dst;
			}

			static inline void put_ne(type& dst, const T& src)
			{
				copy(dst.data, reinterpret_cast<const uchar*>(&src));
			}

			static inline T get_ne(const type& src)
			{
				T result;
				copy(reinterpret_cast<uchar*>(&dst), src.data);
				return result;
			}

			uchar data[Size];
		};

		// Optimization helper (B: storage type; Base: CRTP)
		template <typename T, typename B, typename Base>
		struct endian_buffer_opt
		{
			static inline void put_re(B& dst, const T& src)
			{
				dst = Base::swap(reinterpret_cast<const B&>(src));
			}

			static inline T get_re(const B& src)
			{
				const B value = Base::swap(src);
				return reinterpret_cast<const T&>(value);
			}

			static inline void put_ne(B& dst, const T& src)
			{
				dst = reinterpret_cast<const B&>(src);
			}

			static inline T get_ne(const B& src)
			{
				return reinterpret_cast<const T&>(src);
			}
		};

#if defined(_MSC_VER) || defined(__GNUG__)

		// Optional optimization (may be removed)
		template <typename T>
		struct endian_buffer<T, 2, 2> : endian_buffer_opt<T, std::uint16_t, endian_buffer<T, 2, 2>>
		{
			static_assert(alignof(std::uint16_t) == 2, "Unexpected std::uint16_t alignment");

			using type = std::uint16_t;

			static inline type swap(type src)
			{
#if defined(__GNUG__)
				return __builtin_bswap16(src);
#else
				return _byteswap_ushort(src);
#endif
			}
		};

		// Optional optimization (may be removed)
		template <typename T>
		struct endian_buffer<T, 4, 4> : endian_buffer_opt<T, std::uint32_t, endian_buffer<T, 4, 4>>
		{
			static_assert(alignof(std::uint32_t) == 4, "Unexpected std::uint32_t alignment");

			using type = std::uint32_t;

			static inline type swap(type src)
			{
#if defined(__GNUG__)
				return __builtin_bswap32(src);
#else
				return _byteswap_ulong(src);
#endif
			}
		};

		// Optional optimization (may be removed)
		template <typename T>
		struct endian_buffer<T, 8, 8> : endian_buffer_opt<T, std::uint64_t, endian_buffer<T, 8, 8>>
		{
			static_assert(alignof(std::uint64_t) == 8, "Unexpected std::uint64_t alignment");

			using type = std::uint64_t;

			static inline type swap(type src)
			{
#if defined(__GNUG__)
				return __builtin_bswap64(src);
#else
				return _byteswap_uint64(src);
#endif
			}
		};

#endif

		// Endianness implementation fwd
		template <typename T, std::size_t Align, bool Native>
		class endian_base;

		// Foreign endianness implementation
		template <typename T, std::size_t Align>
		class endian_base<T, Align, false>
		{
			using buf = endian_buffer<T, sizeof(T), Align>;

		public:
			using data_type = typename buf::type;
			using value_type = T;

			data_type data;

			endian_base() = default;

			endian_base(const T& value)
			{
				buf::put_re(data, value);
			}

			endian_base& operator=(const endian_base&) = default;

			endian_base& operator=(const T& value)
			{
				buf::put_re(data, value);
				return *this;
			}

			operator T() const
			{
				return buf::get_re(data);
			}

			T get() const
			{
				return buf::get_re(data);
			}
		};

		// Native endianness implementation
		template <typename T, std::size_t Align>
		class endian_base<T, Align, true>
		{
			using buf = endian_buffer<T, sizeof(T), Align>;

		public:
			using data_type = typename buf::type;
			using value_type = T;

			data_type data;

			endian_base() = default;

			endian_base(const T& value)
			{
				buf::put_ne(data, value);
			}

			endian_base& operator=(const endian_base&) = default;

			endian_base& operator=(const T& value)
			{
				buf::put_ne(data, value);
				return *this;
			}

			operator T() const
			{
				return buf::get_ne(data);
			}

			T get() const
			{
				return buf::get_ne(data);
			}
		};
	}

	template <typename T, std::size_t A = alignof(T)>
	using le = detail::endian_base<T, A, endian::native == endian::little>;

	template <typename T, std::size_t A = alignof(T)>
	using be = detail::endian_base<T, A, endian::native == endian::big>;

	template <typename T, std::size_t A = alignof(T)>
	using ne = detail::endian_base<T, A, true>;

	template <typename T, std::size_t A = alignof(T)>
	using re = detail::endian_base<T, A, false>;
}
