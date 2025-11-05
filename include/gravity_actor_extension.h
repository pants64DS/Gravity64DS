#pragma once
#include "SM64DS_PI.h"
#include "gravity_field.h"
#include "gravity_actor_tree.h"
#include "gravity_math.h"

class ActorExtension : public ActorTreeNode, public ActorList::Node
{
	struct ConverterBase
	{
		const Matrix3x3& basis0;
		const Matrix3x3& basis1;
		const Vector3& pivot;

		ConverterBase(ActorExtension& ext0, const Actor& actor1, const ActorExtension& ext1):
			basis0(ext0.GetGravityMatrix()),
			basis1(ext1.GetGravityMatrix()),
			pivot(actor1.prevPos)
		{}
	};

	struct Converter : private ConverterBase
	{
		using ConverterBase::ConverterBase;

		auto& operator()(Vector3& p) const { return p.RotateAround(pivot, basis1.Transpose()); }
		auto& operator()(short& a)   const { return ConvertAngle(a, basis0, basis1); }
	};

	struct ReverseConverter : private ConverterBase
	{
		using ConverterBase::ConverterBase;

		auto& operator()(Vector3& p) const { return p.RotateAround(pivot, basis1); }
		auto& operator()(short& a)   const { return ConvertAngle(a, basis1, basis0); }
	};

	struct Initializer : public ReverseConverter { using ReverseConverter::ReverseConverter; };
	struct Restorer    : public ReverseConverter { using ReverseConverter::ReverseConverter; };

	template<auto... memberPath>
	class Property
	{
		using T = std::remove_reference_t<decltype((std::declval<Actor>() .* ... .* memberPath))>;

		T original;
		T transformed;

	public:

		void Set(Actor& actor)
		{
			T& val = (actor .* ... .* memberPath);

			original = transformed = val;
		}

		void Set(Actor& actor, const Converter& convert)
		{
			T& val = (actor .* ... .* memberPath);

			original = val;

			transformed = convert(val);
		}

		void Set(Actor& actor, const Initializer& init)
		{
			Set(actor);

			init(original);
		}

		void Set(Actor& actor, const Restorer& restore)
		{
			T& val = (actor .* ... .* memberPath);

			if (val == transformed) [[likely]]
				val = original;
			else [[unlikely]]
				restore(val);
		}

		constexpr const T& GetOriginal() const { return original; }
	};

	template<class... P>
	struct Properties : public P...
	{
		[[gnu::always_inline]]
		void SetAll(Actor& actor, auto&&... f) { (..., static_cast<P&>(*this).Set(actor, f...)); }

		template<auto... memberPath>
		const auto& GetRealValue() const
		{
			return static_cast<const Property<memberPath...>&>(*this).GetOriginal();
		}
	};

	using ActorTreeNode::Find;

	Matrix3x3 currMatrix;
	Vector3 lastUpdatePoint;

	Properties <
		Property<&Actor::pos>,
		Property<&Actor::prevPos>,
		Property<&Actor::ang, &Vector3_16::y>,
		Property<&Actor::motionAng, &Vector3_16::y>
	> properties;

	Sqaerp fieldSqaerp;
	uint16_t angleToNewField = 0;

	int CalculateUpVector(Vector3_Q24& __restrict__ res, const Vector3& pos, Sqaerp& sqaerp) const;

public:

	Matrix3x3 cylClsnPushbackBasis;
	Vector3 savedPos;

	template<auto... memberPath>
	const auto& GetRealValue() const
	{
		return properties.GetRealValue<memberPath...>();
	}

	using ActorList::Node::GetGravityField;
	using ActorList::Node::ShouldBeTransformed;

	void UpdateGravity();

	const Vector3& GetUpVectorQ12() const { return currMatrix.c1; }
	const Vector3& GetLastUpdatePoint() const { return lastUpdatePoint; }
	const Matrix3x3& GetGravityMatrix() const { return currMatrix; }

	bool IsInTrivialField() const
	{
		return GetGravityField().IsTrivial() && angleToNewField == 0;
	}

	[[gnu::noinline]]
	static ActorExtension& Get(const Actor& actor)
	{
		const std::size_t offset = Memory::gameHeapPtr->Sizeof(&actor) - sizeof(ActorExtension);

		return const_cast<ActorExtension&>(
			*reinterpret_cast<const ActorExtension*>(
				reinterpret_cast<const std::byte*>(&actor) + offset
			)
		);
	}

	[[gnu::always_inline]]
	void SetProperties(Actor& pivotActor, const ActorExtension& behavingExtension)
	{
		properties.SetAll(GetActor(), Converter(*this, pivotActor, behavingExtension));
	}

	[[gnu::always_inline]]
	void RestoreProperties(Actor& pivotActor, const ActorExtension& behavingExtension)
	{
		properties.SetAll(GetActor(), Restorer(*this, pivotActor, behavingExtension));
	}

	int PredictNextUpVector(Vector3_Q24& __restrict__ res, const Vector3& nextPos) const
	{
		Sqaerp sqaerpCopy = fieldSqaerp;
		return CalculateUpVector(res, nextPos, sqaerpCopy);
	}

	[[gnu::always_inline]]
	ActorExtension(Actor& actor):
		ActorTreeNode(actor),
		ActorList::Node(actor),
		currMatrix(GetGravityField().GetFirstFieldMatrix(actor.pos, actor.actorID)),
		cylClsnPushbackBasis(Matrix3x3::Identity())
	{
		if (extern Actor* behavingActor; behavingActor && CanSpawnAsSubActor())
			properties.SetAll(actor, Initializer(*this, *behavingActor, ActorExtension::Get(*behavingActor)));
		else
			properties.SetAll(actor);

		lastUpdatePoint = savedPos = GetRealValue<&Actor::pos>();
	}
};

const Actor* ActorCast(const ActorBase& actorBase);

[[gnu::always_inline]]
inline Actor* ActorCast(ActorBase& actorBase)
{
	return const_cast<Actor*>(ActorCast(std::as_const(actorBase)));
}
