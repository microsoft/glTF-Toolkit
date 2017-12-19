#pragma once

#include <DirectXMath.h>

#define PARAM_EXPAND_1(type) type(f)
#define PARAM_EXPAND_2(type) PARAM_EXPAND_1(type) ## , type(f)
#define PARAM_EXPAND_3(type) PARAM_EXPAND_2(type) ## , type(f)
#define PARAM_EXPAND_4(type) PARAM_EXPAND_3(type) ## , type(f)

#define XMTYPE(type, dim)  DirectX::XM ## type ## dim
#define XMSTORE(type, dim) XMStore ## type ## dim
#define XMLOAD(type, dim) XMLoad ## type ## dim

#define XM_OPDEFINE2(op, op_name, dim, type_upper, type_cap, type, add_type)\
XMTYPE(type_upper, dim) operator##op(const XMTYPE(type_upper, dim)& v, add_type f)\
{\
	XMTYPE(type_upper, dim) x = XMTYPE(type_upper, dim)(PARAM_EXPAND_##dim(type));\
	DirectX::XMVECTOR y = DirectX::XMVector##op_name(XMLOAD(type_cap, dim)(&v), XMLOAD(type_cap, dim)(&x));\
\
	XMTYPE(type_upper, dim) r;\
	XMSTORE(type_cap,dim)(&r, y);\
	return r;\
}\
XMTYPE(type_upper, dim) operator##op(add_type f, const XMTYPE(type_upper, dim)& v) { return operator##op(v, f); }

#define XM_OPDEFINE(op, op_name, dim, type_upper, type_cap, type) XM_OPDEFINE2(op, op_name, dim, type_upper, type_cap, type, float) XM_OPDEFINE2(op, op_name, dim, type_upper, type_cap, type, uint32_t)

#define XM_OPDEFINE_ADD(dim, type_upper, type_cap, type) XM_OPDEFINE(+, Add, dim, type_upper, type_cap, type)
#define XM_OPDEFINE_MULT(dim, type_upper, type_cap, type) XM_OPDEFINE(*, Multiply, dim, type_upper, type_cap, type)

#define XMFLOAT_OPDEFINE(dim) XM_OPDEFINE_ADD(dim, FLOAT, Float, float) XM_OPDEFINE_MULT(dim, FLOAT, Float, float)
#define XMUINT_OPDEFINE(dim) XM_OPDEFINE_ADD(dim, UINT, UInt, uint32_t) XM_OPDEFINE_MULT(dim, UINT, UInt, uint32_t)

XMFLOAT_OPDEFINE(2)
XMFLOAT_OPDEFINE(3)
XMFLOAT_OPDEFINE(4)
XMUINT_OPDEFINE(2)
XMUINT_OPDEFINE(3)
XMUINT_OPDEFINE(4)

namespace Microsoft::glTF::Toolkit
{ 
	template <bool _IsVector, typename T>
	struct XMComponentType
	{
		using Type = T;
	};

	template <typename T>
	struct XMComponentType<true, T>
	{
	private:
		template <typename M> static M MemberType(M T::*);

	public:
		using Type = decltype(MemberType(&T::x));
	};

	template <typename T>
	struct XMSerializer
	{
		using TComp = typename XMComponentType<!std::is_fundamental_v<T>, T>::Type;
		static const size_t Dimension = sizeof(T) / sizeof(TComp);

		template <typename U>
		using Normalized = std::conditional<!std::is_integral_v<TComp> && std::is_integral_v<U>, std::true_type, std::false_type>;

		template <typename U, class = std::enable_if_t<Normalized<U>::value>>
		static void Normalize(T& v) { v = v * (1.0f / std::numeric_limits<U>::max()); }
		template <typename U>
		static void Normalize(T& v) { (v); }

		template <typename U, class = std::enable_if_t<Normalized<U>::value>>
		static void Denormalize(T& v) { v = v * std::numeric_limits<U>::max(); }
		template <typename U>
		static void Denormalize(T& v) { (v); }

		static TComp& Get(T& v, size_t Index) { return *((TComp*)&v + Index); }

		template <typename From, size_t _Count>
		static void Create(T& v, const From* Ptr)
		{
			for (size_t i = 0; i < std::min(Dimension, _Count); ++i)
			{
				Get(v, i) = TComp(*(Ptr + i));
			}

			Normalize<From>(v);
		}

		template <typename To, size_t _Count>
		static void Write(To* Ptr, T v)
		{
			Denormalize<To>(v);

			for (size_t i = 0; i < std::min(Dimension, _Count); ++i)
			{
				*(Ptr + i) = (To)Get(v, i);
			}
		}
	};
}
