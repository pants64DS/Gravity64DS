#include "gravity_planet_cam.h"
#include <optional>

constinit const s16 PlanetCam::defaultSettings[8] = { 1000, 250, 1000, 250, 0, 30, 24, 0 };

[[gnu::target("thumb")]]
PlanetCam::PlanetCam(
	CamCtrl* prev,
	const GravityField& field,
	const Camera& cam,
	Player& player,
	ConstructorTag
):
	CamCtrl(std::unique_ptr<CamCtrl>{prev}, field, Type::PLANET_CAM),
	cutsceneTransition(0._fs),
	preCutsceneAngleZ(0),
	camPos(INV_VIEW_MATRIX_ASR_3.c3 << 3),
	touchedGroundAltitude(0._f),
	smoothedGroundAltitude(0._f),
	altitudeInterp(cam.flags & Camera::ZOOMED_OUT ? 1._f : 0._f),
	rotationStopped(false),
	canPlayStopSound(true),
	starSpawning(false)
{
	currSettings = GetSettings(field.GetCamSettingsID());
	if (!currSettings && prev) currSettings = prev->GetPlanetCamSettings();
	if (!currSettings) currSettings = defaultSettings;

	for (u32 i = 0; i < settingInterps.size(); ++i)
		settingInterps[i].Init(currSettings[i]);

	UpdateTransitionSpeeds();

	if (!prev)
	{
		camPos = cam.pos;
		camAltitude = Fix12i::max;
		field.InitBasis(xAxis, yAxis, player.pos);
	}
	else if (prev->GetType() == Type::FIRST_PERSON)
	{
		const Matrix3x3& m = ActorExtension::Get(player).GetGravityMatrix();
		camPos = player.pos - currSettings[MAX_HORZ_DIST] * (Sin(player.ang.y)*m.c0 + Cos(player.ang.y)*m.c2);

		camPos += m.c1 * (
			currSettings[MAX_ALTITUDE_DIFF]
			+ field.GetAltitude(player.pos)
			- field.GetAltitude(camPos)
		);

		camAltitude = field.GetAltitude(camPos);
		yAxis.data = m.c1 << 12;
		xAxis.data = Cos(player.ang.y)*m.c0 - Sin(player.ang.y)*m.c2 << 12;
	}
	else
	{
		camAltitude = field.GetAltitudeAndUpVectorQ24(yAxis, camPos);
		xAxis = yAxis.Cross(Vector3_Q24{INV_VIEW_MATRIX_ASR_3.c1}).Normalized();
	}

	Vector3_Q24 playerUp;
	targetAltitude = field.GetAltitudeAndUpVectorQ24(playerUp, player.pos);

	if ((cam.flags & Camera::ROTATING_LEFT) && (cam.flags & Camera::ROTATING_RIGHT))
		manualReset.Start(*this, player, playerUp.ToQ12());

	if (yAxis.Dot(playerUp) < 0_f24)
	{
		yAxis = playerUp;
		xAxis = yAxis.Cross(Vector3_Q24{INV_VIEW_MATRIX_ASR_3.c1}).Normalized();
		camPos = player.pos + (camAltitude - targetAltitude) * yAxis.ToQ12();
	}
}

static std::optional<CLPS> GetGroundCLPS(Player& player)
{
	RaycastGround ray;
	ray.SetObjAndPos(player.pos, &player);
	ray.DetectClsn();

	if (ray.hadCollision)
		return ray.result.surfaceInfo.clps;
	else
		return std::nullopt;
}

void PlanetCam::UpdateSettings(Player& player, bool warping)
{
	const s16* newSettings = nullptr;

	if (!player.isInAir)
	{
		newSettings = GetSettings(player.wmClsn.sphere.floorResult.surfaceInfo.clps.padding2 & 0xff);
	}
	else if (const auto groundCLPS = GetGroundCLPS(player))
	{
		newSettings = GetSettings(groundCLPS->padding2 & 0xff);

		if (newSettings && !(newSettings[FLAGS] & ACTIVATES_IN_AIR) && !warping)
			newSettings = nullptr;
	}

	if (newSettings && newSettings != currSettings)
	{
		currSettings = newSettings;

		for (u32 i = 0; i < settingInterps.size(); ++i)
			settingInterps[i].SetTarget(currSettings[i]);

		UpdateTransitionSpeeds();
	}

	camAltitude -= GetSetting<MAX_ALTITUDE_DIFF>();

	for (SmoothInterp& setting : settingInterps)
		setting.Advance(settingTransitionSpeed);

	camAltitude += GetSetting<MAX_ALTITUDE_DIFF>();
}

void PlanetCam::UpdateTransitionSpeeds()
{
	settingTransitionSpeed = 1._f / (static_cast<u16>(currSettings[TRANSITION_FRAMES]) + 1);
	altitudeApproachSpeed = currSettings[ALTITUDE_APPROACH_SPEED];
}

[[gnu::target("thumb")]]
void PlanetCam::UpdateCamPos(const Vector3& playerPos, const Vector3& playerUp, Fix12i altitude)
{
	const Vector3 u = playerPos - camPos;
	const Vector3 v = u - u.Dot(playerUp) * playerUp;
	const Fix12i distToAxis = v.Len();

	const Fix12i maxHorzDist = GetSetting<MAX_HORZ_DIST>();
	const Fix12i minHorzDist = manualReset.GetHorzDist();

	if (distToAxis > maxHorzDist)
		camPos += v * (1._f - maxHorzDist / distToAxis);
	else if (distToAxis < minHorzDist)
		camPos += v * (1._f - minHorzDist / distToAxis);

	const Fix12i newAltitude = field->GetAltitudeAndUpVectorQ24(yAxis, camPos);

	xAxis = (xAxis - xAxis.Dot(yAxis) * yAxis).Normalized();
	camPos += yAxis.ToQ12() * (altitude - newAltitude);
}

bool PlanetCam::ApplyRotations(
	const Player& player,
	const Vector3& playerUp,
	short angularVel1,
	short angularVel2)
{
	SphereClsn sphere;
	sphere.flags = BgCh::DETECT_ORDINARY | BgCh::BELONGS_TO_CAMERA;

	if (angularVel1 != 0)
	{
		const Matrix3x3 rotation = Matrix3x3::FromAxisAngle(playerUp, angularVel1);
		const Vector3 newCamPos = rotation(camPos - player.pos) + player.pos;

		sphere.SetObjAndSphere(newCamPos, 200._f, nullptr);
		sphere.DetectClsn();

		if (!(sphere.resultFlags & SphereClsn::COLLISION_EXISTS))
			camPos = newCamPos;
	}
	else
	{
		sphere.SetObjAndSphere(camPos, 200._f, nullptr);
		sphere.DetectClsn();
		camPos += sphere.pushback;
	}

	rotationStopped = sphere.resultFlags & SphereClsn::COLLISION_EXISTS;
	if (rotationStopped) return false;

	if (angularVel2 != 0)
	{
		const Matrix3x3 rotation = Matrix3x3::FromAxisAngle(playerUp, angularVel2);
		xAxis.data *= rotation;
		yAxis.data *= rotation;

		return true;
	}

	return angularVel1 != 0;
}

static u32 GetAngleDistAndDir(const Vector3& v, s16 angle, const Vector3& right, const Vector3& forward)
{
	const Fix12i x = v.Dot(right);
	const Fix12i z = v.Dot(forward);

	u32 res = AngleDiff(Atan2(x, z), angle);

	if (Cos(angle)*x > Sin(angle)*z)
		res |= 1 << 31;

	return res;
}

struct Ratio
{
	int nom, den;

	bool operator<(const Ratio& other) const
	{
		if (this->den == 0) return false;
		if (other.den == 0) return true;

		return this->nom * other.den < other.nom * this->den;
	}
};

template<u32 n>
static void MinimizeRatios(Ratio(&arr)[n])
{
	u32 minIndex = 0;

	for (u32 i = 1; i < n; ++i)
		if (arr[i] < arr[minIndex])
			minIndex = i;

	for (u32 i = 0; i < n; ++i)
		if (i != minIndex)
			arr[i].nom = arr[i].den * arr[minIndex].nom / arr[minIndex].den;
}

bool PlanetCam::ManualResetHandler::Update(
	PlanetCam& planetCam,
	const Camera& cam,
	const Player& player,
	const Vector3& playerUp)
{
	if (inactivityTimer)
	{
		if (INPUT_ARR[0].buttonsHeld & Input::L)
			Start(planetCam, player, playerUp);
		else
		{
			if (inactivityTimer >= 8)
				horzDist = -1;
			else
				++inactivityTimer;

			return false;
		}
	}

	const Vector3 v1 = player.pos - planetCam.camPos;
	const Vector3 v2 = planetCam.xAxis.ToQ12().Cross(planetCam.yAxis.ToQ12());
	const Vector3 playerForward = playerRight.Cross(playerUp).Normalized();
	playerRight = playerUp.Cross(playerForward);

	const Vector3 u = Sin(angle)*playerRight + Cos(angle)*playerForward;
	const Vector3 w = u.Cross(playerUp);

	const u32 res1 = GetAngleDistAndDir(v1, angle, playerRight, playerForward);
	const u32 res2 = GetAngleDistAndDir(v2, angle, playerRight, playerForward);

	bool rotationDir1 = res1 & 1 << 31;
	bool rotationDir2 = res2 & 1 << 31;

	const int maxHorzDist = static_cast<int>(planetCam.GetSetting<MAX_HORZ_DIST>());
	Ratio ratios[] = {
		{0x400, res1 & 0xffff},
		{0x400, res2 & 0xffff},
		{50, maxHorzDist - horzDist}
	};

	int& angularVel1 = ratios[0].nom;
	int& angularVel2 = ratios[1].nom;
	int& angleDist1 = ratios[0].den;
	int& angleDist2 = ratios[1].den;

	if (w.Dot(v1)*w.Dot(v2) < 0_f)
	{
		const bool newRotationDir = v1.Cross(playerUp).Dot(v2) < 0_f;

		if (rotationDir2 != newRotationDir)
		{
			rotationDir2 = newRotationDir;
			angleDist2 = 0x10000 - angleDist2;
		}
	}

	const bool destReached1 = angularVel1 > angleDist1;
	const bool destReached2 = angularVel2 > angleDist2;

	if (destReached1) angularVel1 = angleDist1;
	if (destReached2) angularVel2 = angleDist2;

	MinimizeRatios(ratios);
	const bool destReached3 = ApproachLinear(horzDist, maxHorzDist, ratios[2].nom);

	if (destReached1 && destReached2 && destReached3)
		inactivityTimer = 1;

	if (rotationDir1) angularVel1 = -angularVel1;
	if (rotationDir2) angularVel2 = -angularVel2;

	return planetCam.ApplyRotations(player, playerUp, angularVel1, angularVel2);
}

void PlanetCam::ReactToInput(Camera& cam, const Player& player, const Vector3& playerUp, bool playSounds)
{
	bool rotating = manualReset.Update(*this, cam, player, playerUp);

	if (!manualReset.IsActive())
	{
		rotating = false;
		const u16 buttons = rotationStopped ? INPUT_ARR[0].buttonsPressed : INPUT_ARR[0].buttonsHeld;

		if ((buttons ^ buttons << 1) & Input::CAM_LEFT) // if rotating either left or right, and not both
		{
			short angularVel = 0x400;

			if (buttons & Input::CAM_LEFT)
				angularVel = -angularVel;

			rotating = ApplyRotations(player, playerUp, angularVel, angularVel);
		}
	}

	if (rotating)
	{
		if (playSounds)
			cam.rotationSoundUniqueID = Sound::PlayLong2D(cam.rotationSoundUniqueID, 2, 67, 0);
	}
	else
	{
		manualReset.Stop();

		if (playSounds && canPlayStopSound && rotationStopped)
		{
			Sound::PlayArchive2_2D_Alt(0xe);
			canPlayStopSound = false;
		}
	}
}

bool PlanetCam::CalculateTransform(Matrix4x3& res, Camera& cam, Player& player)
{
	if (cam.flags & 8)
	{
		for (const PowerStar& star : Actor::Iterate<PowerStar>())
		{
			if (!(0 < star.spawnFrameCounter && star.spawnFrameCounter <= 500))
				continue;

			const Vector3& u = ActorExtension::Get(star).GetUpVectorQ12();

			if (!starSpawning)
			{
				const Vector3 relPos = camPos - star.pos;

				Vector3 v = relPos - relPos.Dot(u)*u;
				if (const Fix12i dist = v.Len(); dist < 0x0'010_f)
					v = GetSomeOrthonormalVec(u) * 1000._f;
				else
					v *= 1000._f / dist;

				starSpawnCamPos = star.pos + v + 200*u;
			}

			if (!starSpawning || (star.spawnFrameCounter == 1 && star.unk440 == 2))
				starSpawnLookAt = star.pos;

			const Vector3 offset = starSpawnCamPos - starSpawnLookAt;

			res.c0 = u.Cross(offset).Normalized();
			res.c2 = offset.Normalized();
			res.c1 = res.c2.Cross(res.c0);
			res.c3 = starSpawnCamPos;
			starSpawning = true;

			return true;
		}
	}

	starSpawning = false;

	const bool active = IsActive();
	const bool warping = player.IsWarping();
	if (active) UpdateSettings(player, warping);

	Vector3_Q24 playerUpQ24;
	const Fix12i playerAltitude = field->GetAltitudeAndUpVectorQ24(playerUpQ24, player.pos);
	const Vector3& playerUp = (playerUpQ24.data >>= 12);

	if (player.wmClsn.IsOnGround() && &ActorExtension::Get(player).GetGravityField() == field)
		touchedGroundAltitude = playerAltitude;

	Fix12i groundAltitude;
	if (player.floorY == Fix12i::min)
		groundAltitude = touchedGroundAltitude;
	else
		groundAltitude = playerAltitude - (player.pos.y - player.floorY);

	if (active && !warping)
	{
		const Fix12i maxAltitudeDiff = GetSetting<MAX_ALTITUDE_DIFF>();
		const Fix12i targetAltitude = std::max(touchedGroundAltitude, groundAltitude) + maxAltitudeDiff;

		camAltitude.ApproachLinear(targetAltitude, altitudeApproachSpeed);
		camAltitude = std::clamp(camAltitude, playerAltitude + GetSetting<MIN_ALTITUDE_DIFF>(), playerAltitude + maxAltitudeDiff);

		if (INPUT_ARR[0].buttonsPressed & Input::L)
			Sound::PlayArchive2_2D_Alt(0x1a);
	}

	targetAltitude = std::clamp(targetAltitude, playerAltitude - GetSetting<TARGET_ALTITUDE_RANGE>(), playerAltitude);

	if (INPUT_ARR[0].buttonsPressed & (Input::L | Input::CAM_LEFT | Input::CAM_RIGHT))
		canPlayStopSound = true;

	if (!warping) ReactToInput(cam, player, playerUp, active);

	altitudeInterp.ApproachLinear(cam.flags & Camera::ZOOMED_OUT ? 1._f : 0._f, 0.08_f);
	smoothedGroundAltitude.ApproachLinear(groundAltitude, 50._f);

	const Fix12i altitude = LerpNoinline(
		camAltitude,
		smoothedGroundAltitude + ((camAltitude - smoothedGroundAltitude)*3 >> 1),
		SmoothStep(altitudeInterp)
	);

	UpdateCamPos(player.pos, playerUp, altitude + cam.unk134);

	const Vector3 targetOffset = playerUp * (targetAltitude + GetSetting<TARGET_VERT_OFFSET>() + cam.unk134 - playerAltitude);
	const Vector3_Q24 dir = Vector3_Q24::Raw(camPos - player.pos - targetOffset).NormalizedTwice();

	CalculateRotation(res.Linear(), dir);
	res.c3 = camPos;

	preCutsceneAngleZ = Atan2(playerUp.Dot(res.c0), playerUp.Dot(res.c1));
	cutsceneTransition = 0._fs;

	return !RUNNING_KUPPA_SCRIPT
		&& !(cam.flags & Camera::ZOOMED_IN)
		&& playerUp.Dot(field->GetUpVectorQ12(camPos)) >= 0_f;
}

bool PlanetCam::CanChangeField(const Player& player, const GravityField& newField) const
{
	return field->GetAltitudeVector(camPos) == newField.GetAltitudeVector(camPos);
}

void PlanetCam::AfterCamBehavior(Camera& cam) const
{
	if (manualReset.IsActive())
	{
		cam.flags |= Camera::ROTATING_LEFT | Camera::ROTATING_RIGHT;
	}
	else if (cam.flags & Camera::ARROWS_ALLOWED)
	{
		cam.flags &= ~(Camera::ROTATING_LEFT | Camera::ROTATING_RIGHT);
		if (rotationStopped) return;

		if (INPUT_ARR[0].buttonsHeld & Input::CAM_LEFT)
			cam.flags |= Camera::ROTATING_LEFT;
		else if (INPUT_ARR[0].buttonsHeld & Input::CAM_RIGHT)
			cam.flags |= Camera::ROTATING_RIGHT;
	}
}
