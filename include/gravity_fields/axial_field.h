#pragma once

#include "gravity_math.h"

class AxialField
{
	Vector3 p0;
	Vector3 p1;
	Vector3 v;
	Fix12i radius;

public:
	[[gnu::always_inline]]
	AxialField(const Vector3& p0, const Vector3& p1, Fix12i radius):
		p0(p0), p1(p1),
		v((p1 - p0).Normalized()),
		radius(radius)
	{}

	void CalculateAltitudeVector(Vector3& res, const Vector3& pos) const
	{
		res = pos - p1;
		if (v.Dot(res) > 0_f) return;

		res = pos - p0;
		const Fix12i dot = v.Dot(res);
		if (dot >= 0_f) res -= v*dot;
	}

	bool Contains(const Vector3& pos, Fix12i altitude) const
	{
		return altitude <= radius;
	}
};
