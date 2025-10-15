#pragma once

#include "trivial_field.h"

class TrivialCylinderField : public TrivialField
{
	Vector3 bottomCenter;
	Fix12i radius;
	Fix12i height;

public:
	[[gnu::always_inline]]
	TrivialCylinderField(PathPtr pathPtr):
		bottomCenter(pathPtr.GetNode(0))
	{
		const Vector3 u = pathPtr.GetNode(1);
		radius = bottomCenter.HorzDist(u);
		height = Abs(u.y - bottomCenter.y);
	}

	bool Contains(const Vector3& pos) const
	{
		return bottomCenter.y <= pos.y &&
			pos.y <= bottomCenter.y + height &&
			bottomCenter.HorzDist(pos) <= radius;
	}
};
