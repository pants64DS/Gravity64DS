#pragma once

#include "gravity_math.h"

class HomogeneousCylinderField
{
	Vector3 p0;
	Vector3 p1;
	Vector3 up;
	Fix12i radius;
	Fix12i height;

	Fix12i DistToAxis(const Vector3& pos) const
	{
		const Vector3 relPos = pos - p0;

		return relPos.Dist(relPos.Dot(up) * up);
	}

public:
	HomogeneousCylinderField(PathPtr pathPtr):
		p0(pathPtr.GetNode(0)),
		p1(pathPtr.GetNode(1)),
		up((p1 - p0).Normalized()),
		radius(DistToAxis(pathPtr.GetNode(2))),
		height(p0.Dist(p1))
	{}

	const Vector3& GetUpVector() const
	{
		return up;
	}

	Fix12i GetAltitude(const Vector3& pos) const
	{
		return up.Dot(pos - p0);
	}

	bool Contains(const Vector3& pos, Fix12i altitude) const
	{
		return 0_f <= altitude && altitude <= height &&
			DistToAxis(pos) <= radius;
	}
};
