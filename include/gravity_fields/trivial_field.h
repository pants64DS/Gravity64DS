#pragma once

#include "gravity_math.h"

class TrivialField
{
	static constexpr Vector3 up = {0, 1, 0};

public:
	const Vector3& GetUpVector() const
	{
		return up;
	}

	Fix12i GetAltitude(const Vector3& pos) const
	{
		return pos.y + 30000._f;
	}
};
