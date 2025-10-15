#pragma once
#include "SM64DS_PI.h"
#include "gravity_cam_ctrl.h"

class PlanetCam : public CamCtrl
{
	class ManualResetHandler
	{
		s32 horzDist = -1;
		u8 inactivityTimer = 8;
		s16 angle;
		Vector3 playerRight;

	public:
		bool Update(PlanetCam& planetCam, const Camera& cam, const Player& player, const Vector3& playerUp);
		bool IsActive() const { return inactivityTimer == 0; }
		s32 GetHorzDist() const { return horzDist; }

		void Start(PlanetCam& planetCam, const Player& player, const Vector3& playerUp)
		{
			inactivityTimer = 0;
			const Vector3 v = planetCam.camPos - player.pos;
			horzDist = static_cast<s32>(v.Dist(playerUp*playerUp.Dot(v)));
			angle = player.ang.y;
			playerRight = ActorExtension::Get(player).GetGravityMatrix().c0;
		}

		void Stop()
		{
			if (inactivityTimer == 0)
				inactivityTimer = 1;
		}
	};

	const s16* currSettings;
	std::array<SmoothInterp, 5> settingInterps;
	Fix12i settingTransitionSpeed;
	Fix12i altitudeApproachSpeed;

	enum SettingIDs : u32
	{
		MAX_ALTITUDE_DIFF,
		MIN_ALTITUDE_DIFF,
		MAX_HORZ_DIST,
		TARGET_ALTITUDE_RANGE,
		TARGET_VERT_OFFSET,
		ALTITUDE_APPROACH_SPEED,
		TRANSITION_FRAMES,
		FLAGS,
		NUM_SETTINGS
	};

	enum Flags : s16
	{
		ACTIVATES_IN_AIR = 1 << 0,
	};

	static const s16 defaultSettings[8];

	static const s16* GetSettings(u32 id)
	{
		if (!LEVEL_OVERLAY.planetCamSettingsArray)
			return nullptr;

		const u16 numSettings = *LEVEL_OVERLAY.planetCamSettingsArray;

		if (id < numSettings)
			return LEVEL_OVERLAY.planetCamSettingsArray + 1 + (id << 3);
		else
			return nullptr;
	}

	template<u32 settingID>
	Fix12i GetSetting() const
	{
		return std::get<settingID>(settingInterps).GetValue();
	}

	void UpdateTransitionSpeeds();

	Fix12s cutsceneTransition;
	short preCutsceneAngleZ;

	Vector3 camPos;
	Vector3_Q24 xAxis;
	Vector3_Q24 yAxis;

	Fix12i touchedGroundAltitude;
	Fix12i camAltitude;
	Fix12i targetAltitude;
	Fix12s altitudeInterp;
	bool rotationStopped;
	bool canPlayStopSound;
	ManualResetHandler manualReset;
	bool starSpawning;
	Vector3 starSpawnCamPos;
	Vector3 starSpawnLookAt;

	void CalculateRotation(Matrix3x3& res, const Vector3_Q24& dir)
	{
		SphericalForwardField(res.c1, xAxis, yAxis, dir);

		res.c2 = dir.ToQ12();
		res.c1 >>= 12;
		res.c0 = res.c1.Cross(res.c2).NormalizedTwice();
	}

	void UpdateCamPos(const Vector3& playerPos, const Vector3& playerUp, Fix12i altitude);
	void ReactToInput(Camera& cam, const Player& player, const Vector3& playerUp, bool playSounds);
	void UpdateSettings(Player& player, bool warping);
	bool ApplyRotations(const Player& player, const Vector3& playerUp, short angularVel1, short angularVel2);

	struct ConstructorTag {};
	PlanetCam(CamCtrl* prev, const GravityField& field, const Camera& cam, Player& player, ConstructorTag);

	virtual bool CalculateTransform(Matrix4x3& res, Camera& cam, Player& player) final override;
	virtual bool CanChangeField(const Player& player, const GravityField& newField) const final override;

public:
	virtual void AfterCamBehavior(Camera& cam) const final override;
	virtual const s16* GetPlanetCamSettings() const final override { return currSettings; }

	[[gnu::always_inline]]
	PlanetCam(
		std::unique_ptr<CamCtrl> prev,
		const GravityField& field,
		const Camera& cam,
		Player& player
	):
		PlanetCam(prev.release(), field, cam, player, ConstructorTag{})
	{}

};
