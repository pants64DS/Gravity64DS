#pragma once
#include "gravity_actor_list.h"
#include "gravity_math.h"

class ActorExtension;

class GravityField
{
	GravityField* next = nullptr;
	ActorList actorList;
	s16 priority;
	u8 camSettingsID;
	bool homogeneous : 1;
	bool trivial : 1;

	GravityField(const GravityField&) = delete;
	GravityField(GravityField&&) = delete;
	GravityField& operator=(const GravityField&) = delete;
	GravityField& operator=(GravityField&&) = delete;

	void CalculateFirstFieldMatrix(Matrix3x3& res, const Vector3& pos, uint16_t actorID) const;

	friend class GravityFieldList;
	friend class FieldGenerator;

protected:
	constexpr GravityField():
		priority(-1),
		camSettingsID(0xff),
		homogeneous(true),
		trivial(true)
	{}

	GravityField(PathPtr pathPtr, bool homogeneous, bool trivial = false):
		priority(pathPtr->param2),
		camSettingsID(pathPtr->param3),
		homogeneous(homogeneous),
		trivial(trivial)
	{}

public:
	virtual void CalculateUpVectorQ12(Vector3& res, const Vector3& pos) const = 0;
	virtual void CalculateUpVectorQ24(Vector3& res, const Vector3& pos) const = 0;
	virtual void CalculateAltitudeVector(Vector3& res, const Vector3& pos) const = 0;
	virtual Fix12i GetAltitude(const Vector3& pos) const = 0;
	virtual Fix12i GetAltitudeAndUpVectorQ24(Vector3_Q24& res, const Vector3& pos) const = 0;
	virtual const Vector3* GetHomogeneousUpVectorQ12() const = 0;
	virtual bool Contains(const Vector3& pos) const = 0;
	virtual bool Contains(const Vector3& pos, Fix12i altitude) const = 0;

	void InitBasis(Vector3_Q24& xAxis, Vector3_Q24& yAxis, const Vector3& pos) const;

	static constexpr u8 pathBaseParam1 = 0x40;

	static GravityField& GetFieldAt(const Vector3& pos);
	static GravityField& GetFieldFor(const Actor& actor, const ActorList::Node& node);
	static bool IsPlayerInTrivialField();
	static bool IsPathGravityField(const LevelOverlay::PathObj& path);
	static void Cleanup();

	ActorList& GetActorList() { return actorList; }

	int  GetPriority()      const { return priority; }
	u32  GetCamSettingsID() const { return camSettingsID; }
	bool IsHomogeneous()    const { return homogeneous; }
	bool IsTrivial()        const { return trivial; }

	[[gnu::always_inline]]
	auto GetUpVectorQ12(const Vector3& pos) const
	{
		return Vector3::Proxy([this, &pos]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateUpVectorQ12(res, pos);
		});
	}

	[[gnu::always_inline]]
	auto GetUpVectorQ24(const Vector3& pos) const
	{
		return Vector3_Q24::Proxy([this, &pos]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateUpVectorQ24(res, pos);
		});
	}

	[[gnu::always_inline]]
	auto GetAltitudeVector(const Vector3& pos) const
	{
		return Vector3::Proxy([this, &pos]<bool resMayAlias> [[gnu::always_inline]] (Vector3& res)
		{
			CalculateAltitudeVector(res, pos);
		});
	}

	[[gnu::always_inline]]
	auto GetFirstFieldMatrix(const Vector3& pos, const uint16_t& actorID) const
	{
		return Matrix3x3::Proxy([this, &pos, &actorID]<bool resMayAlias> [[gnu::always_inline]] (Matrix3x3& res)
		{
			CalculateFirstFieldMatrix(res, pos, actorID);
		});
	}

	// Defines an equivalence relation that equates all trivial fields
	bool IsTrivialEquivalent(const GravityField& other) const
	{
		return this == &other || (this->IsTrivial() && other.IsTrivial());
	}
};
