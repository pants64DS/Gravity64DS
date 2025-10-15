#pragma once

#include "gravity_math.h"

class RadialField
{
	Vector3 center;
	Fix12i radius;

public:
	[[gnu::always_inline]]
	RadialField(PathPtr pathPtr):
		center(pathPtr.GetNode(0)),
		radius(center.Dist(pathPtr.GetNode(1)))
	{}

	void CalculateAltitudeVector(Vector3& res, const Vector3& pos) const
	{
		res = pos - center;
	}

	bool Contains(const Vector3& pos, Fix12i altitude) const
	{
		return altitude <= radius;
	}
};
