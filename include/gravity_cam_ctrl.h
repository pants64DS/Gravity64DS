#pragma once
#include <memory>
#include "SM64DS_PI.h"
#include "gravity_actor_extension.h"
#include "gravity_field.h"
#include "gravity_math.h"

class CamCtrl
{
public:
	enum class Type : u8
	{
		VANILLA,
		PLANET_CAM,
		FIRST_PERSON,
		CUTSCENE
	};

private:
	Quaternion rotation = 1;
	Quaternion savedQuat = 1;
	UnitSmoothInterp interp;
	std::unique_ptr<CamCtrl> prev;
	Type type;

	static std::unique_ptr<CamCtrl> latestCam;
	static CamCtrl* activeCam;

	struct MinimapInterp
	{
		s16 angle;
		bool vanillaCamActive;
		bool targetReached;
		bool initialized;
	}
	static inline constinit minimapInterp = {};

	static std::unique_ptr<CamCtrl> Spawn(
		std::unique_ptr<CamCtrl> prev,
		const GravityField& field,
		const Camera& cam,
		Player& player);

	static void UpdatePaused(Camera& cam, const Player& player);
	static void CheckFieldChange(const Camera& cam, Player& player);
	static void Prune();

	void Activate();
	const Quaternion& GetTransformForInterp(Matrix4x3& res, Camera& cam, Player& player);

protected:
	const GravityField* field;

	// Returns whether the camera is still valid
	virtual bool CalculateTransform(Matrix4x3& res, Camera& cam, Player& player)
	{
		res.Linear() = cam.camMat.Linear().Transpose();
		res.c3 = cam.pos;
		return true;
	}

	virtual bool CanChangeField(const Player& player, const GravityField& newField) const
	{
		return newField.IsTrivial();
	}

	bool TryChangeField(const Player& player, const GravityField& newField)
	{
		if (CanChangeField(player, newField))
		{
			field = &newField;
			return true;
		}

		return false;
	}

	bool IsActive() const { return this == activeCam; }

public:
	virtual void AfterCamBehavior(Camera& cam) const {}
	virtual const s16* GetPlanetCamSettings() const { return nullptr; }

	[[gnu::target("thumb")]]
	CamCtrl(
		std::unique_ptr<CamCtrl> prev,
		const GravityField& field,
		Type type
	):
		rotation(prev ? prev->savedQuat.Conjugate()*prev->rotation : Quaternion(1)),
		prev(std::move(prev)),
		type(type),
		field(&field)
	{}

	virtual ~CamCtrl() = default;

	static void Update(Camera& cam, Player& player);
	static const CamCtrl* GetActiveCam() { return activeCam; }
	static bool IsVanillaCamActive();
	static s16 GetMinimapAngle(const Camera& cam);

	static void Cleanup()
	{
		minimapInterp.initialized = false;
		latestCam.reset();
		activeCam = nullptr;
	}

	static bool IsInterpolating()
	{
		return latestCam && latestCam->prev;
	}

	Type GetType() const { return type; }
};
