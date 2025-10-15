#include "gravity_actor_extension.h"
#include "gravity_fields/trivial_field.h"
#include "gravity_fields/radial_field.h"
#include "gravity_fields/axial_field.h"
#include "gravity_fields/homogeneous_cylinder_field.h"
#include "gravity_fields/trivial_cylinder_field.h"
#include <ranges>
#include <span>

template<class Base>
class FieldImpl : public Base, public GravityField
{
	static constexpr bool homogeneous = requires(const Base& base)
	{
		{ base.GetUpVector() } -> std::same_as<const Vector3&>;
	};

	static constexpr bool canCalculateUpVector =
	requires(const Base& base, Vector3& res, const Vector3& pos)
	{
		base.CalculateUpVector(res, pos);
	};

public:
	FieldImpl(PathPtr pathPtr) requires(std::constructible_from<Base, PathPtr>):
		Base(pathPtr),
		GravityField(pathPtr, homogeneous, std::is_base_of_v<TrivialField, Base>)
	{}

	template<class... Args>
	FieldImpl(PathPtr pathPtr, Args&&... args):
		Base(std::forward<Args>(args)...),
		GravityField(pathPtr, homogeneous, std::is_base_of_v<TrivialField, Base>)
	{}

	FieldImpl() = default;

	virtual void CalculateUpVectorQ12(Vector3& res, const Vector3& pos) const final override
	{
		if constexpr (homogeneous)
			res = Base::GetUpVector();
		else if constexpr (canCalculateUpVector)
			Base::CalculateUpVector(res, pos);
		else
		{
			FieldImpl<Base>::CalculateAltitudeVector(res, pos);
			res.Normalize();
		}
	}

	virtual void CalculateUpVectorQ24(Vector3& res, const Vector3& pos) const final override
	{
		if constexpr (homogeneous)
			res = Base::GetUpVector() << 12;
		else if constexpr (canCalculateUpVector)
		{
			Base::CalculateUpVector(res, pos);
			res <<= 12;
		}
		else
		{
			FieldImpl<Base>::CalculateAltitudeVector(res, pos);
			Vector3_Q24::NormalizeInPlace(res);
		}
	}

	virtual void CalculateAltitudeVector(Vector3& res, const Vector3& pos) const final override
	{
		if constexpr (requires(Base base) { Base::CalculateAltitudeVector(res, pos); })
			Base::CalculateAltitudeVector(res, pos);
		else
		{
			const Fix12i altitude = Base::GetAltitude(pos);

			if constexpr (requires(Base base) { base.CalculateAltitudeVector(res, pos, altitude); })
				Base::CalculateAltitudeVector(res, pos, altitude);
			else
			{
				FieldImpl<Base>::CalculateUpVectorQ12(res, pos);
				res *= altitude;
			}
		}
	}

	virtual Fix12i GetAltitudeAndUpVectorQ24(Vector3_Q24& res, const Vector3& pos) const final override
	{
		if constexpr (homogeneous)
		{
			res.data = Base::GetUpVector() << 12;
			return Base::GetAltitude(pos);
		}
		else if constexpr (canCalculateUpVector)
		{
			Base::CalculateUpVector(res.data, pos);
			res <<= 12;
			return Base::GetAltitude(pos);
		}
		else if constexpr (requires(Fix12i altitude)
			{ Base::CalculateAltitudeVector(res, pos, altitude); })
		{
			const Fix12i altitude = Base::GetAltitude(pos);
			Base::CalculateAltitudeVector(res.data, pos, altitude);
			res /= Fix24i{altitude, as_raw};
			return altitude;
		}
		else
		{
			Base::CalculateAltitudeVector(res.data, pos);
			const Fix12i altitude = res.data.Len();
			res /= Fix24i{altitude, as_raw};
			return altitude;
		}
	}

	virtual const Vector3* GetHomogeneousUpVectorQ12() const final override
	{
		if constexpr (homogeneous)
			return &Base::GetUpVector();
		else
			return nullptr;
	}

	virtual Fix12i GetAltitude(const Vector3& pos) const final override
	{
		if constexpr (requires { Base::GetAltitude(pos); })
			return Base::GetAltitude(pos);
		else
		{
			Vector3 v;
			Base::CalculateAltitudeVector(v, pos);
			return v.Len();
		}
	}

	virtual bool Contains(const Vector3& pos) const final override
	{
		if constexpr (requires(Base base) { base.Contains(pos); })
			return Base::Contains(pos);
		else
		{
			const Fix12i altitude = FieldImpl<Base>::GetAltitude(pos);
			return Base::Contains(pos, altitude);
		}
	}

	virtual bool Contains(const Vector3& pos, Fix12i altitude) const final override
	{
		if constexpr (requires(Base base) { base.Contains(pos, altitude); })
			return Base::Contains(pos, altitude);
		else
			return Base::Contains(pos);
	}
};

struct DefaultGravityField : public TrivialField
{
	DefaultGravityField() = default;
	DefaultGravityField(PathPtr pathPtr) {}

	static constexpr uint8_t minRequiredPathNodes = 0;

	bool Contains(const Vector3& pos) const
	{
		return true;
	}
};

static constinit FieldImpl<DefaultGravityField> defaultGravityField;

bool GravityField::Contains(const Vector3& pos) const
{
	return true;
}

Fix12i GravityField::GetAltitude(const Vector3& pos) const
{
	return pos.y + 30000._f;
}

struct FieldGenerator
{
	uintptr_t sizeCounter;
	GravityField** nextPtr;

	template<class F, class... Args>
	void Spawn(Args&&... args)
	{
		using G = FieldImpl<F>;
		static_assert(std::is_trivially_destructible_v<G>);
		static_assert(alignof(G) == alignof(GravityField));

		if (nextPtr)
		{
			std::byte* dest = reinterpret_cast<std::byte*>(sizeCounter);
			*nextPtr = new (dest) G (std::forward<Args>(args)...);
			nextPtr = &(*nextPtr)->next;
		}

		sizeCounter += sizeof(G);
	}

	void Generate(PathPtr pathPtr)
	{
		const auto numNodes = pathPtr.NumNodes();
		if (numNodes < 2) return;

		switch (pathPtr->param1 - GravityField::pathBaseParam1)
		{
		case 0:
			if (numNodes == 2)
				Spawn<RadialField>(pathPtr);
			else
			{
				const unsigned lastNodeID = pathPtr.NumNodes() - 1;
				const Fix12i radius = pathPtr.GetNode(lastNodeID)
					.Dist(pathPtr.GetNode(lastNodeID - 1));

				Vector3 p0 = pathPtr.GetNode(0);

				for (unsigned i = 1; i < lastNodeID; ++i)
				{
					const Vector3 p1 = pathPtr.GetNode(i);
					Spawn<AxialField>(pathPtr, p0, p1, radius);
					p0 = p1;
				}
			}
			break;
		case 1:
			if (numNodes >= 3)
				Spawn<HomogeneousCylinderField>(pathPtr);
			break;
		case 2:
			Spawn<TrivialCylinderField>(pathPtr);
			break;
		}
	}
};

static_assert(alignof(GravityField) == __STDCPP_DEFAULT_NEW_ALIGNMENT__, "Compile with the -faligned-new=4 flag");

class GravityFieldList
{
	std::byte* storage = nullptr;
	GravityField* root = nullptr;

	[[gnu::target("thumb")]]
	void Fill()
	{
		if (storage || !ROOT_ACTOR_BASE || ROOT_ACTOR_BASE->actorID != 3)
			return;

		const unsigned numPaths = NUM_PATHS;

		const LevelOverlay::PathObj* pathArray[numPaths]; // i wish this was standard
		const LevelOverlay::PathObj** nextPtr = &pathArray[0];

		size_t size = 0;

		for (const PathPtr pathPtr : std::span(PathPtr(0u).ptr, numPaths))
		{
			if (pathPtr->param1 < GravityField::pathBaseParam1)
				continue;

			FieldGenerator generator = {0, nullptr};
			generator.Generate(pathPtr);

			if (generator.sizeCounter != 0)
			{
				size += generator.sizeCounter;
				*nextPtr++ = pathPtr;
			}
		}

		std::ranges::subrange gravityFieldPaths(&pathArray[0], nextPtr);
		if (gravityFieldPaths.empty()) return;

		InsertionSort(gravityFieldPaths, [](const LevelOverlay::PathObj* path0, const LevelOverlay::PathObj* path1)
		{
			return path0->param2 > path1->param2;
		});

		storage = new std::byte[size];
		FieldGenerator generator = {reinterpret_cast<uintptr_t>(storage), &root};

		for (const PathPtr pathPtr : gravityFieldPaths)
			generator.Generate(pathPtr);
	}

	class Iterator
	{
		GravityField* ptr;
	public:
		constexpr Iterator(GravityField* ptr) : ptr(ptr) {}

		Iterator& operator++()
		{
			ptr = ptr->next;

			return *this;
		}

		GravityField& operator* () const { return *ptr; }
		GravityField* operator->() const { return ptr; }

		constexpr bool operator==(const Iterator& other) const = default;
	};

public:
	Iterator begin() { Fill(); return root; }
	Iterator end() const { return nullptr; }
	void Clear() { delete[] storage; storage = nullptr; root = nullptr; }
}
static constinit fieldList;

void GravityField::Cleanup()
{
	fieldList.Clear();
}

GravityField& GravityField::GetFieldFor(const Actor& actor, const ActorList::Node& node)
{
	if (node.AlwaysInDefaultField())
		return defaultGravityField;
	else
		return GetFieldAt(actor.pos);
}

GravityField& GravityField::GetFieldAt(const Vector3& pos)
{
	// The list is already sorted by priority from high to low
	// No subsequent field in the list will have a higher priority
	auto it = fieldList.begin();
	while (true)
	{
		if (it == fieldList.end())
			return defaultGravityField;

		if (it->Contains(pos))
			break;

		++it;
	}

	const int priority = it->GetPriority();
	Fix12i lowestAltitude = it->GetAltitude(pos);
	GravityField* lowestAltitudeField = &*it;

	while (++it != fieldList.end() && it->priority == priority)
	{
		const Fix12i altitude = it->GetAltitude(pos);

		if (lowestAltitude > altitude && it->Contains(pos, altitude))
		{
			lowestAltitude = altitude;
			lowestAltitudeField = &*it;
		}
	}

	return *lowestAltitudeField;
}

bool GravityField::IsPlayerInTrivialField()
{
	if (PLAYER_ARR[0]) [[likely]]
		return ActorExtension::Get(*PLAYER_ARR[0]).IsInTrivialField();

	else [[unlikely]]
		return true;
}

[[gnu::target("thumb")]]
void GravityField::CalculateFirstFieldMatrix(Matrix3x3& res, const Vector3& pos, uint16_t actorID) const
{
	if (IsTrivial())
	{
		res = Matrix3x3::Identity();
		return;
	}

	const Vector3_Q24 upAxis = GetUpVectorQ24(pos);
	Vector3_Q24 xAxis, yAxis;

	if (actorID == 0xbf)
		InitBasis(xAxis, yAxis, pos);
	else
	{
		AssureUnaliased(xAxis) = Vector3_Q24::Temp(1._f24, 0._f24, 0._f24);
		AssureUnaliased(yAxis) = Vector3_Q24::Temp(0._f24, upAxis.GetY() < 0_f24 ? -1._f24 : 1._f24, 0._f24);
	}

	SphericalMatrixField(res, xAxis, yAxis, upAxis);
}

[[gnu::target("thumb")]]
void GravityField::InitBasis(Vector3_Q24& xAxis, Vector3_Q24& yAxis, const Vector3& pos) const
{
	const int viewID = ENTRANCE_ARR_PTR[LAST_ENTRANCE_ID].param1 >> 3 & 0xf;
	const LevelOverlay::ViewObj& view = GetViewObj(viewID);

	AssureUnaliased(yAxis) = GetUpVectorQ24(view.pos);

	AssureUnaliased(xAxis) = yAxis.Cross(Vector3_Q24::Raw (
		pos.x - view.pos.x,
		pos.y - view.pos.y,
		pos.z - view.pos.z
	)).Normalized();
}

constexpr uint8_t maxGravityFieldID = 0x10;

bool GravityField::IsPathGravityField(const LevelOverlay::PathObj& path)
{
	const u8 base = GravityField::pathBaseParam1;

	return base <= path.param1 && path.param1 <= base + maxGravityFieldID;
}
