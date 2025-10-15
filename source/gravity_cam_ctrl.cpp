#include "gravity_actor_extension.h"
#include "gravity_field.h"
#include "gravity_planet_cam.h"
#include "gravity_first_person_cam.h"
#include "gravity_cutscene_cam.h"

constinit std::unique_ptr<CamCtrl> CamCtrl::latestCam = nullptr;
constinit CamCtrl* CamCtrl::activeCam = nullptr;

std::unique_ptr<CamCtrl> CamCtrl::Spawn(
	std::unique_ptr<CamCtrl> prev,
	const GravityField& field,
	const Camera& cam,
	Player& player
)
{
	if (field.IsTrivial())
		return std::make_unique<CamCtrl>(std::move(prev), field, Type::VANILLA);
	else if (RUNNING_KUPPA_SCRIPT)
		return std::make_unique<CutsceneCam>(std::move(prev), field, player);
	else if (cam.flags & Camera::ZOOMED_IN)
		return std::make_unique<FirstPersonCam>(std::move(prev), field, player);
	else
		return std::make_unique<PlanetCam>(std::move(prev), field, cam, player);
}

void CamCtrl::Activate()
{
	activeCam = this;
	CamCtrl* ptr = this;

	while (true)
	{
		ptr = ptr->prev.get();
		if (!ptr) break;
		ptr->interp.SetDirectionForward();
	}
}

void CamCtrl::CheckFieldChange(const Camera& cam, Player& player)
{
	const GravityField& currField = ActorExtension::Get(player).GetGravityField();

	if (!activeCam)
	{
		latestCam = Spawn(std::move(latestCam), currField, cam, player);
		latestCam->Activate();
		return;
	}

	if (activeCam->field->IsTrivialEquivalent(currField))
		return;

	if (activeCam->TryChangeField(player, currField))
		return;

	for (auto* camCtrl = latestCam->prev.get(); camCtrl; camCtrl = camCtrl->prev.get())
	{
		if (camCtrl->field->IsTrivialEquivalent(currField))
		{
			camCtrl->interp.SetDirectionBackward();
			camCtrl->Activate();

			return;
		}
	}

	latestCam = Spawn(std::move(latestCam), currField, cam, player);
	latestCam->Activate();
}

void CamCtrl::Prune()
{
	// If the active camera has finished its interp, delete all cameras newer than it
	if (activeCam->interp.IsFinished())
	{
		for (auto* ptr = &latestCam; *ptr; ptr = &(*ptr)->prev)
		{
			if (ptr->get() == activeCam)
			{
				latestCam = std::move(*ptr);
				break;
			}
		}
	}

	// Delete all inactive cameras that have finished their interp in the forward direction
	auto* ptr = &latestCam;
	while (*ptr)
	{
		CamCtrl& camCtrl = **ptr;

		if (&camCtrl != activeCam && camCtrl.interp.IsDirectionForward() && camCtrl.interp.IsFinished())
			*ptr = std::move(camCtrl.prev);
		else
			ptr = &(*ptr)->prev;
	}
}

const Quaternion& CamCtrl::GetTransformForInterp(
	Matrix4x3& res,
	Camera& cam,
	Player& player)
{
	const bool valid = CalculateTransform(res, cam, player);
	if (!valid && this == activeCam)
		activeCam = nullptr;

	Quaternion_FromMatrix3x3(rotation, res.Linear(), auto(rotation));
	return rotation;
}

static void SetCamMatrix(Camera& cam, Matrix4x3&& transform)
{
	cam.camMat = transform.RigidInverse();
	cam.camMat.c3 >>= 3;
}

static const LevelOverlay::ViewObj* GetPauseView()
{
	for (int i = 0; i < NUM_VIEWS; i++)
	{
		const LevelOverlay::ViewObj& view = GetViewObj(i);

		if (view.mode == LevelOverlay::ViewObj::PAUSE_CAMERA_CENTER_POINT)
			return &view;
	}

	return nullptr;
}

void CamCtrl::UpdatePaused(Camera& cam, const Player& player)
{
	if (!activeCam->field->IsTrivial())
	{
		const LevelOverlay::ViewObj* pauseView = GetPauseView();

		if (pauseView)
		{
			cam.camMat.c2 = player.pos - pauseView->pos;

			if (const Fix12i dist = cam.camMat.c2.Len(); dist >= 1._f)
				cam.camMat.c2 /= dist;
			else
				cam.camMat.c2 = ActorExtension::Get(player).GetUpVectorQ12();

			cam.camMat.c1 = GetSomeOrthonormalVec(cam.camMat.c2);
			cam.camMat.c0 = cam.camMat.c1.Cross(cam.camMat.c2);
			cam.camMat.c3 = (player.pos + (cam.camMat.c2 * 6000)) >> 3;
			cam.camMat = cam.camMat.RigidInverse();

			return;
		}
	}

	cam.camMat = VIEW_MATRIX_ASR_3;
}

#ifdef GRAVITY_DEBUG_COUNTERS
constinit unsigned cylClsnUpdateCounter = 0;
constinit unsigned behaviorTransformCounter = 0;
constinit unsigned activeActorCounter = 0;
constinit unsigned bgChTransformCounter = 0;
#endif

void CamCtrl::Update(Camera& cam, Player& player)
{
#ifdef GRAVITY_DEBUG_COUNTERS
	ShowDecimalInt(cylClsnUpdateCounter, 10, 10);
	ShowDecimalInt(behaviorTransformCounter, 10, 40);
	ShowDecimalInt(activeActorCounter, 10, 70);
	ShowDecimalInt(bgChTransformCounter, 10, 100);

	cylClsnUpdateCounter = 0;
	behaviorTransformCounter = 0;
	activeActorCounter = 0;
	bgChTransformCounter = 0;
#endif

	CheckFieldChange(cam, player);
	Prune();

	if (GAME_PAUSED)
	{
		UpdatePaused(cam, player);
		return;
	}

	auto* prev = latestCam->prev.get();

	if (!prev)
	{
		Matrix4x3 currTransform;
		latestCam->GetTransformForInterp(currTransform, cam, player);
		if (latestCam->type == Type::VANILLA) return;

		SetCamMatrix(cam, std::move(currTransform));

		return;
	}

	Matrix4x3 currTransform;
	Quaternion currRotation = latestCam->GetTransformForInterp(currTransform, cam, player);
	currTransform.c3 -= player.pos;

	do
	{
		Matrix4x3 prevTransform;
		const Fix12i t = prev->interp.NextValue(0.03_f);
		if (prev->interp.IsFinished()) prev->savedQuat = 1;

		const Quaternion& q0 = prev->GetTransformForInterp(prevTransform, cam, player);
		const Quaternion& q1 = currRotation;
		prevTransform.c3 -= player.pos;

		Quaternion r = Quaternion::FromVector3(prevTransform.c3, currTransform.c3);
		if (r.Dot(prev->savedQuat) < 0_f) r = -r;

		prev->savedQuat = r;
		r *= t;

		const Fix12i s = 1._f - t;
		currRotation = (s + r)*(s*q0 + r.Conjugate()*q1);
		currRotation.Normalize();
		currTransform.c3 = Lerp(prevTransform.c3, currTransform.c3, t);
	}
	while (prev = prev->prev.get());

	currTransform.Linear() = Matrix3x3::FromQuaternion(currRotation);
	currTransform.c3 += player.pos;
	SetCamMatrix(cam, std::move(currTransform));
}

// Change the camera matrix before the view matrices are set
void repl_0200de68(Camera& cam)
{
	if (cam.owner && cam.owner->actorID == 0xbf)
		CamCtrl::Update(cam, static_cast<Player&>(*cam.owner));

	cam.View::Render();
}

asm(R"(
nsub_0200e2e8:
	mov     r0, r4
	pop     {r4, r5, r14}
	b       AfterCamBehavior
)");

extern "C" s32 AfterCamBehavior(Camera& cam)
{
	if (const CamCtrl* activeCam = CamCtrl::GetActiveCam())
		activeCam->AfterCamBehavior(cam);

	return 1;
}

extern "C" void PlaySoundWhenVanillaCamActive(u32 soundID)
{
	if (CamCtrl::IsVanillaCamActive())
		Sound::UnkPlaySoundFunc(soundID);
}

asm("repl_0200a548 = PlaySoundWhenVanillaCamActive"); // manual reset start sound
asm("repl_0200ae30 = PlaySoundWhenVanillaCamActive"); // cam hitting wall sound

u32 repl_0200ae64(u32 uniqueID, u32 archiveID, u32 soundID, u32 arg4) // cam rotating sound
{
	if (CamCtrl::IsVanillaCamActive())
		return Sound::PlayLong2D(uniqueID, archiveID, soundID, arg4);
	else
		return uniqueID;
}

bool CamCtrl::IsVanillaCamActive()
{
	return !activeCam || activeCam->type == Type::VANILLA;
}

asm(R"(
nsub_020faa5c_ov_02:
	mov     r0, r4
	bl      _ZN7CamCtrl15GetMinimapAngleERK6Camera
	mov     r2, r0
	b       0x020faa60
)");

s16 CamCtrl::GetMinimapAngle(const Camera& cam)
{
	const bool vanillaCamActive = CamCtrl::IsVanillaCamActive();
	const s16 targetAngle = vanillaCamActive ? cam.angY : 0;
	MinimapInterp& mi = minimapInterp;

	if (!mi.initialized)
	{
		mi.angle = targetAngle;
		mi.vanillaCamActive = vanillaCamActive;
		mi.targetReached = true;
		mi.initialized = true;

		return targetAngle;
	}

	if (vanillaCamActive != mi.vanillaCamActive) mi.targetReached = false;
	if (mi.targetReached) return mi.angle = targetAngle;

	const s32 diff = ApproachAngle(mi.angle, targetAngle, 1, 6_deg, 6_deg);
	if (diff == 0) mi.targetReached = true;

	mi.vanillaCamActive = vanillaCamActive;

	return mi.angle;
}
