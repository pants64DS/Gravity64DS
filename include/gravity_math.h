#pragma once
#include <ranges>
#include "SM64DS_PI.h"

struct Vector3_Q24;

extern "C"
{
	int DivQ24(int num, int den);
	uint64_t Div64(uint64_t nom, uint64_t den);
}

template<FixUR T>
struct Fix24 : Fix<T, 24, Fix24>
{
	using Fix<T, 24, Fix24>::Fix;

	Fix24<int> operator/ (Fix24 fix) const { return {DivQ24(this->val, fix.val), as_raw}; }
	Fix24&     operator/=(Fix24 fix) &     { this->val = DivQ24(this->val, fix.val); return *this; }

	template<typename U> constexpr explicit
	operator Fix12<U>() const { return {(this->val + 0x800) >> 12, as_raw}; }
};

using Fix24i = Fix24<int>;

constexpr Fix24i operator""_f24 (unsigned long long val) { return Fix24i(val, as_raw); }
constexpr Fix24i operator""_f24 (long double val)        { return Fix24i(val); }

inline s16 Atan2(Fix24i y, Fix24i x) { return Atan2(y.val, x.val); }

[[nodiscard, gnu::noinline]]
inline Fix12i LerpNoinline(Fix12i a, Fix12i b, Fix12i t)
{
	return Lerp(a, b, t);
}

struct Vector3_Q24
{
	Vector3 data;

	[[gnu::noinline]]
	static void NormalizeInPlace(Vector3& v) { CalculateNormalized(v, v); }
	static void Multiply(Vector3& res, const Vector3& v, Fix24i scalar);
	static Fix24i CalculateDot(const Vector3& v0, const Vector3& v1);
	static void CalculateCross(Vector3& res, const Vector3& v0, const Vector3& v1);
	static void CalculateNormalized(Vector3& res, const Vector3& v);

	template<class F>
	class Proxy
	{
		F eval;

		template<class G>
		using NewProxy = Proxy<G>;

	public:
		[[gnu::always_inline]]
		constexpr explicit Proxy(F&& eval) : eval(eval) {}

		template<bool resMayAlias> [[gnu::always_inline]]
		void Eval(Vector3& res) { eval.template operator()<resMayAlias>(res); }

		template<bool resMayAlias> [[gnu::always_inline]]
		void Eval(Vector3_Q24& res) { Eval<resMayAlias>(res.data); }

		[[gnu::always_inline, nodiscard]]
		Fix24i Len() && { return static_cast<Vector3_Q24>(std::move(*this)).Len(); }

		template<class G> [[gnu::always_inline, nodiscard]]
		Fix24i Dot(Proxy<G>&& proxy) &&
		{
			return CalculateDot (
				static_cast<Vector3_Q24>(std::move(*this)).data,
				static_cast<Vector3_Q24>(std::move(proxy)).data
			);
		}

		[[gnu::always_inline, nodiscard]]
		Fix24i Dot(const Vector3_Q24& other) &&
		{
			return CalculateDot(static_cast<Vector3_Q24>(std::move(*this)).data, other.data);
		}

		[[gnu::always_inline, nodiscard]]
		auto AngleTo(const Vector3_Q24& other) &&
		{
			return static_cast<Vector3_Q24>(std::move(*this)).AngleTo(other);
		}

		[[gnu::always_inline, nodiscard]]
		auto Cross(const Vector3_Q24& v) &&
		{
			return NewProxy([this, &v]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				if constexpr (resMayAlias)
				{
					const Vector3 temp = v.data;
					Eval<resMayAlias>(res);
					CalculateCross(res, res, temp);
				}
				else
				{
					Eval<resMayAlias>(res);
					CalculateCross(res, res, v.data);
				}
			});
		}

		template<class G> [[gnu::always_inline, nodiscard]]
		auto Cross(Proxy<G>&& other) &&
		{
			return NewProxy([this, &other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				CalculateCross(res, res, std::move(other).StoreAsQ12());
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto Normalized() &&
		{
			return NewProxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				NormalizeInPlace(res);
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto NormalizedTwice() &&
		{
			return NewProxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				NormalizeInPlace(res);
				NormalizeInPlace(res);
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto StoreAsQ12() &&
		{
			return Vector3::Proxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto operator+(const Vector3_Q24& v) && { return v + std::move(*this); }

		template<class G> [[gnu::always_inline, nodiscard]]
		auto operator+(Proxy<G>&& other) &&
		{
			return NewProxy([this, &other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				res += std::move(other).StoreAsQ12();
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto operator-(const Vector3_Q24& v) &&
		{
			return NewProxy([this, &v]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				if constexpr (resMayAlias)
				{
					const Vector3 temp = v.data;
					Eval<resMayAlias>(res);
					res -= temp;
				}
				else
				{
					Eval<resMayAlias>(res);
					res -= v.data;
				}
			});
		}

		template<class G> [[gnu::always_inline, nodiscard]]
		auto operator-(Proxy<G>&& other) &&
		{
			return NewProxy([this, &other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				res -= std::move(other).StoreAsQ12();
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto operator*(const Fix24i& scalar) &&
		{
			return NewProxy([this, &scalar]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				Multiply(res, res, scalar);
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto operator<<(const int& shift) &&
		{
			return NewProxy([this, &shift]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				res <<= shift;
			});
		}

		[[gnu::always_inline, nodiscard]]
		auto operator>>(const int& shift) &&
		{
			return NewProxy([this, &shift]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
			{
				Eval<resMayAlias>(res);
				res >>= shift;
			});
		}
	};

	constexpr Vector3_Q24() = default;

	constexpr Vector3_Q24(auto x, auto y, auto z, auto... flags)
	{
		SetX({x, flags...});
		SetY({y, flags...});
		SetZ({z, flags...});
	}

	template<class F> [[gnu::always_inline]]
	Vector3_Q24(Proxy<F>&& proxy) { proxy.template Eval<false>(data); }

	template<class F> [[gnu::always_inline]]
	Vector3_Q24(Vector3::Proxy<F>&& proxy, AsRaw) { proxy.template Eval<false>(data); }

	Vector3_Q24(const auto& v, AsRaw) : data(v) {}
	explicit Vector3_Q24(const Vector3& v) : data(v << 12) {}
	
	template<class F> [[gnu::always_inline]]
	Vector3_Q24& operator=(Proxy<F>&& proxy) & { proxy.template Eval<true>(data); return *this; }

	constexpr Fix24i GetX() const { return {data.x.val, as_raw}; }
	constexpr Fix24i GetY() const { return {data.y.val, as_raw}; }
	constexpr Fix24i GetZ() const { return {data.z.val, as_raw}; }

	constexpr void SetX(Fix24i fix) { data.x.val = fix.val; }
	constexpr void SetY(Fix24i fix) { data.y.val = fix.val; }
	constexpr void SetZ(Fix24i fix) { data.z.val = fix.val; }

	[[gnu::always_inline]]
	bool operator==(const Vector3_Q24& other) const& = default;

	// use an inlinable version if either operand is a proxy
	template<class T, class P> [[gnu::always_inline]] friend
	bool operator== (T&& any, Proxy<P>&& proxy)
	{
		const Vector3_Q24& v = std::forward<T>(any);

		return v.data == std::move(proxy).StoreAsQ12();
	}

	Fix24i Len    () const { return {LenVec3     (data), as_raw}; }
	Fix24i HorzLen() const { return {Vec3_HorzLen(data), as_raw}; }

	[[gnu::always_inline, nodiscard]]
	constexpr auto ToQ12() const
	{
		return Vector3::Proxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			(*[](Vector3& res, const Vector3_Q24& source)
			{
				res.x = static_cast<Fix12i>(source.GetX());
				res.y = static_cast<Fix12i>(source.GetY());
				res.z = static_cast<Fix12i>(source.GetZ());
			})
			(res, *this);
		});
	}

	[[gnu::always_inline, nodiscard]]
	static constexpr auto Temp(const Fix24i& x, const Fix24i& y, const Fix24i& z)
	{
		return Proxy([&x, &y, &z]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			res.x.val = x.val;
			res.y.val = y.val;
			res.z.val = z.val;
		});
	}

	[[gnu::always_inline, nodiscard]]
	static constexpr auto Raw(const auto& x, const auto& y, const auto& z)
	{
		return Proxy([&x, &y, &z]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			res.x.val = x.val;
			res.y.val = y.val;
			res.z.val = z.val;
		});
	}

	[[gnu::always_inline, nodiscard]]
	static constexpr auto Raw(const auto& v)
	{
		return Proxy([&v]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			const auto& [x, y, z] = v;

			res.x.val = x.val;
			res.y.val = y.val;
			res.z.val = z.val;
		});
	}

	template<class F> [[gnu::always_inline, nodiscard]]
	static  constexpr auto Raw(Vector3::Proxy<F>&& proxy)
	{
		return Proxy([&proxy]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			proxy.template Eval<resMayAlias>(res);
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto Cross(const Vector3_Q24& other) const
	{
		return Proxy([this, &other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateCross(res, data, other.data);
		});
	}

	template<class F> [[gnu::always_inline, nodiscard]]
	auto Cross(Proxy<F>&& proxy) const
	{
		return Proxy([this, &proxy]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			if constexpr (resMayAlias)
			{
				const Vector3 temp = data;
				proxy.template Eval<resMayAlias>(res);
				CalculateCross(res, temp, res);
			}
			else
			{
				proxy.template Eval<resMayAlias>(res);
				CalculateCross(res, data, res);
			}
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto Normalized() const
	{
		return Proxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateNormalized(res, this->data);
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto NormalizedTwice() const
	{
		return Proxy([this]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateNormalized(res, this->data);
			NormalizeInPlace(res);
		});
	}

	template<class F>
	Fix24i Dot(Proxy<F>&& proxy) const { return CalculateDot(this->data, static_cast<Vector3_Q24>(std::move(proxy)).data); }
	Fix24i Dot(const Vector3_Q24& v) const { return CalculateDot(this->data, v.data); }
	void Normalize()      & { NormalizeInPlace(data); }
	void NormalizeTwice() & { NormalizeInPlace(data);  NormalizeInPlace(data); }
	
	int AngleTo(const Vector3_Q24& other) const
	{
		return Atan2(this->Cross(other).Len(), this->Dot(other)) & 0xffff;
	}

	void SetMaxAngleToNormalized(const Vector3_Q24& v, short ang);

	friend Vector3_Q24& operator+=(Vector3_Q24& v0, const Vector3_Q24& v1) { AddVec3(v0.data, v1.data, v0.data); return v0; }
	friend Vector3_Q24& operator-=(Vector3_Q24& v0, const Vector3_Q24& v1) { SubVec3(v0.data, v1.data, v0.data); return v0; }

	Vector3_Q24& operator*=(Fix24i scalar) &
	{
		Multiply(data, data, scalar);

		return *this;
	}

	Vector3_Q24& operator/=(Fix24i scalar) &
	{
		SetX(GetX() / scalar);
		SetY(GetY() / scalar);
		SetZ(GetZ() / scalar);

		return *this;
	}

	Vector3_Q24& operator<<=(int shift) & { data <<= shift; return *this; }
	Vector3_Q24& operator>>=(int shift) & { data >>= shift; return *this; }

	[[gnu::always_inline, nodiscard]]
	auto operator<<(const int& shift) const
	{
		return Proxy([this, &shift]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			Vec3_Lsl(res, data, shift);
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto operator>>(const int& shift) const
	{
		return Proxy([this, &shift]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			Vec3_Asr(res, data, shift);
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto operator+(const Vector3_Q24& other) const
	{
		return Proxy([this, other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			AddVec3(this->data, other.data, res);
		});
	}

	template<class F> [[gnu::always_inline, nodiscard]]
	auto operator+(Proxy<F>&& proxy) const
	{
		return Proxy([this, &proxy]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			if constexpr (resMayAlias)
			{
				const Vector3 temp = data;
				proxy.template Eval<resMayAlias>(res);
				res += temp;
			}
			else
			{
				proxy.template Eval<resMayAlias>(res);
				res += data;
			}
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto operator-(const Vector3_Q24& other) const
	{
		return Proxy([this, &other]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			SubVec3(this->data, other.data, res);
		});
	}

	template<class F> [[gnu::always_inline, nodiscard]]
	auto operator-(Proxy<F>&& proxy) const
	{
		return Proxy([this, &proxy]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			if constexpr (resMayAlias)
			{
				const Vector3 temp = data;
				proxy.template Eval<resMayAlias>(res);
				SubVec3(temp, res, res);
			}
			else
			{
				proxy.template Eval<resMayAlias>(res);
				SubVec3(data, res, res);
			}
		});
	}

	[[gnu::always_inline, nodiscard]]
	auto operator*(const Fix24i& scalar) const
	{
		return Proxy([this, &scalar]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			Multiply(res, data, scalar);
		});
	}

	template<class F>
	[[gnu::always_inline, nodiscard]] friend auto operator* (const Fix24i& scalar, Proxy<F>&& proxy) { return std::move(proxy) * scalar; }
	[[gnu::always_inline, nodiscard]] friend auto operator* (const Fix24i& scalar, const Vector3_Q24& v) { return v * scalar; }
};

struct Matrix3x3_Q24
{
    Vector3_Q24 c0;
    Vector3_Q24 c1;
    Vector3_Q24 c2;
};

inline const ostream& operator<<(const ostream& os, Fix24i fix)
{
	ostream::set_buffer("0x%r0%_f24");
	ostream::flush(fix.val);

	return os;
}

inline const ostream& operator<<(const ostream& os, const Vector3_Q24& vec)
{
	ostream::set_buffer("{0x%r0%_f24, 0x%r1%_f24, 0x%r2%_f24}");
	ostream::flush(vec.GetX().val, vec.GetY().val, vec.GetZ().val);

	return os;
}

[[gnu::const]] Fix24i Sqrt(Fix24i x);
[[gnu::const]] Fix24i CosQ24(short a);

inline Fix12i IterateSmoothStep(Fix12i t, unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		t = SmoothStep(t);

	return t;
}

void SphericalForwardField(Vector3& __restrict__ res, const Vector3_Q24& xAxis, const Vector3_Q24& yAxis, const Vector3_Q24& up);

inline void SphericalMatrixField(Matrix3x3& __restrict__ res, const Vector3_Q24& xAxis, const Vector3_Q24& yAxis, const Vector3_Q24& up)
{
	SphericalForwardField(res.c2, xAxis, yAxis, up);

	Vector3_Q24::CalculateCross(res.c0, up.data, res.c2);

	res.c0.NormalizeTwice();
	res.c1 = up.data.NormalizedTwice();
	res.c2.NormalizeTwice();
}

void CalculateSomeOrthonormalVec(Vector3& res, const Vector3& v);

inline auto GetSomeOrthonormalVec(const Vector3& v)
{
	return Vector3::Proxy([&v]<bool resMayAlias>[[gnu::always_inline]](Vector3& res)
	{
		CalculateSomeOrthonormalVec(res, v);
	});
};

[[gnu::always_inline]]
inline short GetAngleOffset(const Vector3& oldZAxis, const Vector3& newXAxis, const Vector3& newZAxis)
{
	return Atan2(newXAxis.Dot(oldZAxis), newZAxis.Dot(oldZAxis));
}

[[gnu::noclone]]
inline short& ConvertAngle(short& angle, const Matrix3x3& fromBasis, const Matrix3x3& toBasis)
{
	return angle += GetAngleOffset(fromBasis.c2, toBasis.c0, toBasis.c2);
}

inline Fix24i SinQ24(short a)
{
	return CosQ24(90_deg - a);
}

class Sqaerp // "spherical quadratically adaptive interpolation"
{
	short angularVel = 0;
public:
	// returns the new angle
	int operator()(Vector3_Q24& v, const Vector3_Q24& target, short angularAccel, bool brake = false, int maxAngle = 0x8000);

	constexpr void Reset() & { angularVel = 0; }
};

template<std::bidirectional_iterator Iter>
void InsertionSort(Iter begin, Iter end, auto&& cmp)
{
	if (begin == end) return;

	Iter firstUnsorted = begin;

	while (++firstUnsorted != end)
	{
		Iter insertPos = firstUnsorted;

		while (insertPos > begin && cmp(*insertPos, *std::prev(insertPos)))
		{
			std::swap(*insertPos, *std::prev(insertPos));
			--insertPos;
		}
	}
}

template<std::ranges::bidirectional_range Range, class Cmp>
void InsertionSort(Range&& range, Cmp&& cmp)
{
	InsertionSort(std::ranges::begin(range), std::ranges::end(range), std::forward<Cmp>(cmp));
}

// Interpolates from the identity matrix to the rotation
// matrix m with slightly varying angular velocity
void RotationInterp(Matrix3x3& m, Fix12i t);

// Interpolates between the rotation matrices m0
// and m1 with slightly varying angular velocity
inline void InterpRotations(
	const Matrix3x3& m0,
	const Matrix3x3& m1,
	Fix12i t,
	Matrix3x3& __restrict__ res) // res can't alias m0 or m1
{
	res = m0.Transpose();
	res = m1 * res;
	RotationInterp(res, t);
	res = res * m0;
}

// assumes that m is a rotation matrix and res doesn't alias hemisphere
void Quaternion_FromMatrix3x3(
	Quaternion& res,
	const Matrix3x3& m,
	const Quaternion& hemisphere);

// Like SmoothStep but allows switching direction and setting any target value
class SmoothInterp
{
	Fix12i h1;
	Fix12i h;
	Fix12i k;
	Fix12i t;

	void Init(Fix12i start, Fix12i end, Fix12i speed)
	{
		h1 = end;
		h = start - end;
		k = speed;
		t = 0;
	}

public:
	void Init(Fix12i val) { Init(val, val, 0); }
	SmoothInterp(Fix12i val) { Init(val); }
	SmoothInterp() = default;

	Fix12i GetValue() const
	{
		return Sqr(t - 1._f)*((k + (h << 1))*t + h) + h1;
	}

	Fix12i GetSpeed() const
	{
		return (t - 1)*(3*(k + (h << 1))*t - k);
	}

	Fix12i GetTarget() const { return h1; }

	void SetTarget(Fix12i val)
	{
		Init(GetValue(), val, GetSpeed());
	}

	void Advance(Fix12i deltaTime)
	{
		t.ApproachLinear(1._f, deltaTime);
	}
};

// Like SmoothStep but allows switching direction
class UnitSmoothInterp
{
	Fix12s t = 0;
	Fix12s end = 1;
	bool complement = true;
	Fix12i leadingCoeff = 2;

public:
	Fix12i NextValue(Fix12i deltaTime)
	{
		t.ApproachLinear(end, deltaTime);
		const Fix12i u = t - end;
		const Fix12i val = u*u*(u*leadingCoeff + 3);
		return complement ? 1 - val : val;
	}

	void SwitchDirection()
	{
		const Fix12i u = t - end;
		const Fix12i v = u * leadingCoeff;
		const Fix12i w = v >> 1;

		end = Sqrt(u*u*(w*w - 2) + 1) - u*(w + 1);
		leadingCoeff = (2 - u*(6*(end + u) + v*(3*end + 2*u))) / (end*end*end);
		t = 0;
		complement = !complement;
	}

	bool IsFinished() const { return t == end; }
	bool IsDirectionForward() const { return complement; }

	void SetDirectionForward()
	{
		if (!IsDirectionForward())
			SwitchDirection();
	}

	void SetDirectionBackward()
	{
		if (IsDirectionForward())
			SwitchDirection();
	}
};
