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

// std::le_t<T,A> -- Little Endian
// std::be_t<T,A> -- Big Endian
// std::endian_base<T,A,Native> -- LE/BE implementation (selected by `Native`)
// First template argument is the underlying type (base: arithmetic or enum).
// Second optional template argument is explicit alignment (native by default).
// Alignment may be increased and decreased.
// Setting greater alignment works similar to alignas() and isn't very useful.
// Setting small alignment (especially 1) is the alternative to `#pragma pack`.

// std::be_t<std::uint32_t> -- big endian uint32_t
// std::le_t<std::uint32_t> -- little endian uint32_t
// std::be_t<std::uint32_t, 2> -- big endian uint32_t with enforced alignment 2 (packing)

#pragma once

#include <type_traits>
#include <cstdint>
#include <cstring>

namespace std
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
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || defined(_M_ARM)
		native = little,
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
#else
#error "Unknown endianness"
#endif
	};

	template <typename T>
	struct has_endianness : std::integral_constant<bool, std::is_arithmetic<T>::value || std::is_enum<T>::value>
	{
	};

	namespace detail
	{
		using uchar = unsigned char;

		// Copy with byteswap (fallback algorithm)
		template <std::size_t Size>
		inline void revert(uchar* dst, const uchar* src)
		{
			for (std::size_t i = 0; i < Size; i++)
			{
				dst[i] = src[Size - 1 - i];
			}
		}

		template <typename T, std::size_t Size = sizeof(T), std::size_t Align = 1>
		struct alignas(Align) endian_buffer
		{
			using type = endian_buffer;

			static constexpr bool can_opt = (Size == 2 || Size == 4 || Size == 8) && Align != Size;

			static inline void put_re(type& dst, const T& src)
			{
				if (can_opt)
				{
					endian_buffer<T, Size, Size> buf_opt;
					buf_opt.put_re(buf_opt, src);
					std::memcpy(dst.data, &buf_opt, Size);
					return;
				}

				revert<Size>(dst.data, reinterpret_cast<const uchar*>(&src));
			}

			static inline T get_re(const type& src)
			{
				if (can_opt)
				{
					endian_buffer<T, Size, Size> buf_opt;
					std::memcpy(&buf_opt, src.data, Size);
					return buf_opt.get_re(buf_opt);
				}

				T dst;
				revert<Size>(reinterpret_cast<uchar*>(&dst), src.data);
				return dst;
			}

			static inline void put_ne(type& dst, const T& src)
			{
				std::memcpy(dst.data, reinterpret_cast<const uchar*>(&src), Size);
			}

			static inline T get_ne(const type& src)
			{
				T dst;
				std::memcpy(reinterpret_cast<uchar*>(&dst), src.data, Size);
				return dst;
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

			operator const B&() const
			{
				return data;
			}

			operator B&()
			{
				return data;
			}

			B data;
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
	}

#ifdef __BIG_ENDIAN__
#define LE_STORE put_re
#define LE_LOAD get_re
#define BE_STORE put_ne
#define BE_LOAD get_ne
#else
#define LE_STORE put_ne
#define LE_LOAD get_ne
#define BE_STORE put_re
#define BE_LOAD get_re
#endif

	template <typename T>
	void le_store(void* dst, const T& value)
	{
		static_assert(has_endianness<T>::value, "le_store<>: invalid type");
		using buf = detail::endian_buffer<T>;
		buf::LE_STORE(*static_cast<typename buf::type*>(dst), value);
	}

	template <typename T>
	void le_load(T& value, const void* src)
	{
		static_assert(has_endianness<T>::value, "le_load<>: invalid type");
		using buf = detail::endian_buffer<T>;
		value = buf::LE_LOAD(*static_cast<const typename buf::type*>(src));
	}

	template <typename T>
	T le_load(const void* src)
	{
		static_assert(has_endianness<T>::value, "le_load<>: invalid type");
		using buf = detail::endian_buffer<T>;
		return buf::LE_LOAD(*static_cast<const typename buf::type*>(src));
	}

	template <typename T>
	void be_store(void* dst, const T& value)
	{
		static_assert(has_endianness<T>::value, "be_store<>: invalid type");
		using buf = detail::endian_buffer<T>;
		buf::BE_STORE(*static_cast<typename buf::type*>(dst), value);
	}

	template <typename T>
	void be_load(T& value, const void* src)
	{
		static_assert(has_endianness<T>::value, "be_load<>: invalid type");
		using buf = detail::endian_buffer<T>;
		value = buf::BE_LOAD(*static_cast<const typename buf::type*>(src));
	}

	template <typename T>
	T be_load(const void* src)
	{
		static_assert(has_endianness<T>::value, "be_load<>: invalid type");
		using buf = detail::endian_buffer<T>;
		return buf::BE_LOAD(*static_cast<const typename buf::type*>(src));
	}

#undef LE_STORE
#undef LE_LOAD
#undef BE_STORE
#undef BE_LOAD

	// Endianness support type
	template <typename T, std::size_t Align, bool Native>
	class endian_base
	{
		static_assert(has_endianness<T>::value, "endian_base<>: invalid type");

		using buf = detail::endian_buffer<T, sizeof(T), Align>;
		using data_t = typename buf::type;

		data_t data;

	public:
		using value_type = T;

		endian_base() = default;

		endian_base(const T& value)
		{
			Native ? buf::put_ne(data, value) : buf::put_re(data, value);
		}

		endian_base& operator=(const endian_base&) = default;

		endian_base& operator=(const T& value)
		{
			Native ? buf::put_ne(data, value) : buf::put_re(data, value);
			return *this;
		}

		operator T() const
		{
			return Native ? buf::get_ne(data) : buf::get_re(data);
		}

		T get() const
		{
			return Native ? buf::get_ne(data) : buf::get_re(data);
		}

		auto operator++(int)
		{
			auto val = get();
			auto result = val++;
			*this = val;
			return result; // Forward
		}

		auto operator--(int)
		{
			auto val = get();
			auto result = val--;
			*this = val;
			return result; // Forward
		}

		endian_base& operator++()
		{
			auto val = get();
			++val;
			return (*this = val);
		}

		endian_base& operator--()
		{
			auto val = get();
			--val;
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator+=(T2&& rhs)
		{
			auto val = get();
			val += std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator-=(T2&& rhs)
		{
			auto val = get();
			val -= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator*=(T2&& rhs)
		{
			auto val = get();
			val *= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator/=(T2&& rhs)
		{
			auto val = get();
			val /= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator%=(T2&& rhs)
		{
			auto val = get();
			val %= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator&=(T2&& rhs)
		{
			auto val = get();
			val &= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator|=(T2&& rhs)
		{
			auto val = get();
			val |= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator^=(T2&& rhs)
		{
			auto val = get();
			val ^= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator<<=(T2&& rhs)
		{
			auto val = get();
			val <<= std::forward<T2>(rhs);
			return (*this = val);
		}

		template <typename T2>
		endian_base& operator>>=(T2&& rhs)
		{
			auto val = get();
			val >>= std::forward<T2>(rhs);
			return (*this = val);
		}
	};

	template <typename T, std::size_t A = alignof(T)>
	using le_t = endian_base<T, A, endian::native == endian::little>;

	template <typename T, std::size_t A = alignof(T)>
	using be_t = endian_base<T, A, endian::native == endian::big>;
}
