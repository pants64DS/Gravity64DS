#include "gravity_actor_extension.h"
#include "gravity_field.h"
#include "gravity_cam_ctrl.h"

int repl_0202cae0() // Clean up resources before changing levels
{
	GravityField::Cleanup();
	CamCtrl::Cleanup();

	return 0x91c; // the hook replaces ldr r0,=0x91c
}

asm(R"(
@ Remove fall damage
nsub_020e2d08_ov_02 = 0x020e2da0
nsub_020e2da8_ov_02 = 0x020e2e68

@ Prevent up/downwarps
nsub_020c06e8_ov_02:
	strlo r2,[r0, #0x60]
	movlo r0, #1
	movhs r0, #0
	bx    lr

@ Fix a glitch that allows the player to jump mid-air after walking off a cliff
@ when there's no collision triangle below them and their Y-coordinate is positive
nsub_020d3874_ov_02:
	blo   0x020d3894
	b     0x020d3878

@ Fix a glitch that allows the player to long jump mid-air after sliding off a cliff
@ in the crounch state when there's no collision triangle below them
nsub_020d1378_ov_02:
	add   r0, #0x32000
	cmp   r1, r0
	b     0x020d1380

nsub_020e50c4_ov_02:
nsub_020e50e4_ov_02:
	mov   r0, r5
	ldr   r14, =0x020e5100
	b     _Z16AdjustInputAngleR6Player
)");

void AdjustInputAngle(Player& player)
{
	const auto& extension = ActorExtension::Get(player);

	if (extension.IsInTrivialField())
	{
		player.inputAngle += GetAngleToCamera(player.playerID);
		return;
	}

	Vector3 forward;
	SphericalForwardField (
		forward,
		static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c0),
		static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c2),
		Vector3_Q24::Raw(extension.GetUpVectorQ12() << 12).Normalized()
	);

	forward >>= 12;

	const auto& currMatrix = extension.GetGravityMatrix();

	player.inputAngle += GetAngleOffset(forward, currMatrix.c0, currMatrix.c2);
}

asm(R"(
repl_020f0b4c_ov_02:
	str  r0,[r13,#8]
	mov  r0, r4
	mov  r1, r13
	b    _Z28UpdateNumberModelTranslationRK6NumberR7Vector3
)");

void UpdateNumberModelTranslation(const Number& number, Vector3& translation)
{
	if (GravityField::IsPlayerInTrivialField())
		return;
	
	const Fix12i dist = translation.Dist(number.spawnPos);

	AssureUnaliased(translation) = number.spawnPos + (dist * INV_VIEW_MATRIX_ASR_3.c1) + INV_VIEW_MATRIX_ASR_3.c2 * 300._f;
}

// Set the transform of the number model
void repl_020f0bbc(Matrix4x3& transform, Fix12i x, Fix12i y, Fix12i z)
{
	transform.c3 = Vector3::Temp(x, y, z);

	if (GravityField::IsPlayerInTrivialField())
		transform.Linear() = Matrix3x3::Identity();
	else
		transform.Linear() = INV_VIEW_MATRIX_ASR_3.Linear();
}

asm(R"(
repl_020c4e20_ov_02:
	mov  r1, r6
	add  r14, r14, #4
	b    _Z21CalculatePlayerLookAtR7Vector3RK6PlayersRKS_
)");

void CalculatePlayerLookAt(Vector3& res, const Player& player, short angY, const Vector3& v)
{
	auto& extension = ActorExtension::Get(player);

	if (extension.IsInTrivialField())
		Vec3_RotateYAndTranslate(res, player.pos, angY, v);
	else
	{
		Vec3_RotateYAndTranslate (res,
			extension.GetRealValue<&Actor::pos>(),
			extension.GetRealValue<&Actor::ang, &Vector3_16::y>(),
			v
		);
	}
}

asm(R"(
nsub_0214bbc8_ov_66:
	mov     r0, r4
	pop     {r4, r5, r14}
	b       AfterBobOmbReset
)");

extern "C" void AfterBobOmbReset(const Actor& bobOmb)
{
	// Make Bob-ombs respawn in the right position
	ActorExtension::Get(bobOmb).savedPos = bobOmb.pos;
}

void RedirectDelta(Actor& actor, ActorExtension& extension);

bool repl_020e4458_ov_02(Player& player) // before updating the player's model transform
{
	auto& extension = ActorExtension::Get(player);
	RedirectDelta(player, extension);

	const bool held = player.UpdateBeingHeld();
	if (held) extension.savedPos = player.pos;

	return held;
}

asm(R"(
nsub_020e4228_ov_02:
	mov     r0, r4
	bl      TransformCarryMatrix
	mov     r0, #0x500
	orr     r0, #0x0bc
	b       0x020e422c
)");

extern "C" void TransformCarryMatrix(Player& player)
{
	const auto& playerExtension = ActorExtension::Get(player);
	const auto& holdingExtension = ActorExtension::Get(*player.holdingActor);
	const Matrix3x3& g0 = holdingExtension.GetGravityMatrix();
	const Matrix3x3& g1 = playerExtension.GetGravityMatrix();
	const Vector3 pivot = holdingExtension.GetRealValue<&Actor::pos>() >> 3;

	MATRIX_SCRATCH_PAPER.Linear() = g1.Transpose()(g0(MATRIX_SCRATCH_PAPER.Linear()));
	MATRIX_SCRATCH_PAPER.c3.RotateAround(pivot, g0);
}

asm(R"(
repl_020b1810_ov_02:
	mov     r2, r5
	b       SpawnRedCoinStar
)");

extern "C" Actor* SpawnRedCoinStar(u32 actorID, u32 param1, const StarMarker& marker, const Vector3_16* rot, s32 areaID, s32 deathTableID)
{
	const Vector3 pos = marker.pos + ActorExtension::Get(marker).GetUpVectorQ12() * 120._f;

	return Actor::Spawn(actorID, param1, pos, rot, areaID, deathTableID);
}

asm(R"(
repl_020eaa7c_ov_02:
	mov     r1, r5
	b       IsStarAwayFromMarker
)");

extern "C" bool IsStarAwayFromMarker(const Vector3& starPos, const StarMarker& marker)
{
	const Vector3 u = ActorExtension::Get(marker).GetUpVectorQ12();
	const Vector3 relPos = starPos - marker.pos;
	return relPos != static_cast<s32>(relPos.Dot(u) + 0.5_f)*u;
}
