#include "gravity_math.h"

void Vector3_Q24::Multiply(Vector3& res, const Vector3& v, Fix24i scalar)
{
	res.x.val = (Fix24i(v.x.val, as_raw) * scalar).val;
	res.y.val = (Fix24i(v.y.val, as_raw) * scalar).val;
	res.z.val = (Fix24i(v.z.val, as_raw) * scalar).val;
}

void Vector3_Q24::CalculateNormalized(Vector3& res, const Vector3& v)
{
	const Fix24i mag (v.Len(), as_raw);
	res.x.val = (Fix24i(v.x, as_raw) / mag).val;
	res.y.val = (Fix24i(v.y, as_raw) / mag).val;
	res.z.val = (Fix24i(v.z, as_raw) / mag).val;
}

Fix24i Vector3_Q24::CalculateDot(const Vector3& v0, const Vector3& v1)
{
	return Fix24i(v0.x, as_raw) * Fix24i(v1.x, as_raw)
	     + Fix24i(v0.y, as_raw) * Fix24i(v1.y, as_raw)
	     + Fix24i(v0.z, as_raw) * Fix24i(v1.z, as_raw);
}

void Vector3_Q24::CalculateCross(Vector3& res, const Vector3& v0, const Vector3& v1)
{
	const Fix24i x0(v0.x, as_raw);
	const Fix24i y0(v0.y, as_raw);
	const Fix24i z0(v0.z, as_raw);
	const Fix24i x1(v1.x, as_raw);
	const Fix24i y1(v1.y, as_raw);
	const Fix24i z1(v1.z, as_raw);

	res.x.val = (y0 * z1 - z0 * y1).val;
	res.y.val = (z0 * x1 - x0 * z1).val;
	res.z.val = (x0 * y1 - y0 * x1).val;
}

void Vector3_Q24::SetMaxAngleToNormalized(const Vector3_Q24& j, short angle)
{
	const Fix24i dot = this->Dot(j);
	const Fix24i cosine = CosQ24(angle);

	if (cosine < dot) return;

	*this = ((*this - j * dot).Normalized() * SinQ24(angle) + j * cosine).Normalized();
}

int Sqaerp::operator()(Vector3_Q24& v, const Vector3_Q24& target, short angularAccel, bool brake, int maxAngle)
{
	if (angularAccel <= 0)
		return false;
	
	const int currAngle = std::min<int>(v.AngleTo(target), maxAngle);
	const int expectedMidAngle = angularVel * angularVel / (angularAccel << 1);

	if (currAngle > expectedMidAngle && !brake)
		angularVel -= angularAccel;
	else
		angularVel = std::min<short>(angularVel + angularAccel, 0);
	
	if (-angularVel < currAngle)
	{
		const auto newAngle = currAngle + angularVel;
		v.SetMaxAngleToNormalized(target, newAngle);

		return newAngle;
	}

	angularVel = 0;
	v = target;

	return 0;
}

consteval uint64_t fac(uint64_t n)
{
	return n ? n * fac(n - 1) : 1;
}

Fix24i CosQ24(short a)
{
	int b = a;
	if (b < 0) b = -b;
	
	bool flipped;

	if (b > 0x4000)
	{
		b = 0x8000 - b;
		flipped = true;
	}
	else flipped = false;

	const Fix24i x = {b << 10, as_raw};
	const Fix24i res = 1._f24 - (0x1'41ff5e_f24 - (0x0'2be0ed_f24*x + 0x0'161e72_f24)*x)*x*x;

	return flipped ? -res : res;
}

void SphericalForwardField(Vector3& __restrict__ res, const Vector3_Q24& xAxis, const Vector3_Q24& yAxis, const Vector3_Q24& up)
{
	AssureUnaliased(res) = xAxis.Cross(up).StoreAsQ12();
	Vector3_Q24 u = yAxis.Cross(up);

	if (u.Dot(u) > 0x00'000100_f24)
	{
		u.Normalize();

		const Fix24i a = Vector3_Q24::CalculateDot(res, u.data);
		const Fix24i b = a * a - Vector3_Q24::CalculateDot(res, res) + 1._f24;

		u *= Sgn(up.Dot(xAxis)) * Sqrt(std::max(b, 0._f24)) + a;
		res -= u.data;
	}

	Vector3_Q24::NormalizeInPlace(res);
}

// Assumes v is a unit vector
void CalculateSomeOrthonormalVec(Vector3& res, const Vector3& v)
{
	const int xx = v.x.val * v.x.val;
	const int yy = v.y.val * v.y.val;
	const int zz = v.z.val * v.z.val;

	if (xx < yy)
	{
		if (xx < zz)
		{
			res = Vector3::Temp(0._f, v.z, -v.y).NormalizedTwice();

			return;
		}
	}
	else if (yy < zz)
	{
		res = Vector3::Temp(-v.z, 0._f, v.x).NormalizedTwice();

		return;
	}

	res = Vector3::Temp(v.y, -v.x, 0._f).NormalizedTwice();
}

// Interpolates from the identity matrix to the rotation
// matrix m with slightly varying angular velocity
void RotationInterp(Matrix3x3& m, Fix12i t)
{
	Fix12i traces[4] =
	{
		+ m.c0.x + m.c1.y + m.c2.z,
		+ m.c0.x - m.c1.y - m.c2.z,
		- m.c0.x + m.c1.y - m.c2.z,
		- m.c0.x - m.c1.y + m.c2.z,
	};

	unsigned j = 0;
	for (unsigned i = 1; i < 4; ++i)
		if (traces[j] < traces[i]) j = i;

	Fix12i a = 1._f + traces[j];
	SqrtAsync(std::max(a, 0_f) << 2);
	Quaternion q;

	switch (j)
	{
	case 0:
		q.w = a;
		q.x = m.c1.z - m.c2.y;
		q.y = m.c2.x - m.c0.z;
		q.z = m.c0.y - m.c1.x;
		break;
	case 1:
		q.w = m.c1.z - m.c2.y;
		q.x = a;
		q.y = m.c0.y + m.c1.x;
		q.z = m.c2.x + m.c0.z;
		break;
	case 2:
		q.w = m.c2.x - m.c0.z;
		q.x = m.c0.y + m.c1.x;
		q.y = a;
		q.z = m.c1.z + m.c2.y;
		break;
	default:
		q.w = m.c0.y - m.c1.x;
		q.x = m.c2.x + m.c0.z;
		q.y = m.c1.z + m.c2.y;
		q.z = a;
		break;
	}

	Fix12i s = 1 - t;

	a = SqrtResultFix12i();
	if (q.w < 0_f) a = -a;

	Fix12i r = (t << 1) / (t*s*(q.w - a << 1) + a);
	Fix12i b = r*t*a >> 1;

	m.c0.x = b*(m.c0.x - 1._f) + 1._f;
	m.c1.y = b*(m.c1.y - 1._f) + 1._f;
	m.c2.z = b*(m.c2.z - 1._f) + 1._f;

	r *= s;
	Fix12i c;

	c = r*q.z;
	m.c0.y = b*m.c0.y + c;
	m.c1.x = b*m.c1.x - c;

	c = r*q.y;
	m.c0.z = b*m.c0.z - c;
	m.c2.x = b*m.c2.x + c;

	c = r*q.x;
	m.c1.z = b*m.c1.z + c;
	m.c2.y = b*m.c2.y - c;
}

// assumes that m is a rotation matrix and res doesn't alias hemisphere
void Quaternion_FromMatrix3x3(
	Quaternion& res,
	const Matrix3x3& m,
	const Quaternion& hemisphere)
{
	Fix12i traces[4] =
	{
		+ m.c0.x + m.c1.y + m.c2.z,
		+ m.c0.x - m.c1.y - m.c2.z,
		- m.c0.x + m.c1.y - m.c2.z,
		- m.c0.x - m.c1.y + m.c2.z,
	};

	unsigned j = 0;
	for (unsigned i = 1; i < 4; ++i)
		if (traces[j] < traces[i]) j = i;

	Fix12i a = 1._f + traces[j];
	SqrtAsync(std::max(a, 0_f) << 2);

	switch (j)
	{
	case 0:
		res.w = a;
		res.x = m.c1.z - m.c2.y;
		res.y = m.c2.x - m.c0.z;
		res.z = m.c0.y - m.c1.x;
		break;
	case 1:
		res.w = m.c1.z - m.c2.y;
		res.x = a;
		res.y = m.c0.y + m.c1.x;
		res.z = m.c2.x + m.c0.z;
		break;
	case 2:
		res.w = m.c2.x - m.c0.z;
		res.x = m.c0.y + m.c1.x;
		res.y = a;
		res.z = m.c1.z + m.c2.y;
		break;
	case 3:
		res.w = m.c0.y - m.c1.x;
		res.x = m.c2.x + m.c0.z;
		res.y = m.c1.z + m.c2.y;
		res.z = a;
		break;
	}

	SqrtResultFix12i().InverseAsync();
	const Fix12i dot = res.Dot(hemisphere);

	a = HardwareDivResultQ12();
	if (dot < 0_f) a = -a;
	res *= a;
}
