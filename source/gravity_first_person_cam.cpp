#include "gravity_first_person_cam.h"

bool FirstPersonCam::CanChangeField(const Player& player, const GravityField& newField) const
{
	return !newField.IsTrivial();
}

bool FirstPersonCam::CalculateTransform(Matrix4x3& res, Camera& cam, Player& player)
{
	if (!(cam.flags & Camera::ZOOMED_IN) || player.currState != &Player::ST_FIRST_PERSON)
		valid = false;

	if (valid)
	{
		if (!IsInterpolating())
		{
			angX += INPUT_ARR[0].dirZ.val >> 3;
			angX = std::clamp<s16>(angX, -0x1c00, 0x1c00);
			angY += player.ang.y - playerAngY - (INPUT_ARR[0].dirX.val >> 3);
		}

		cam.unk17e = angX;
		cam.angY = angY;

		const short angleDiff = player.ang.y - 180_deg - angY;

		if (angleDiff < -45_deg) player.ang.y = angY + 135_deg;
		if (angleDiff >  45_deg) player.ang.y = angY - 135_deg;

		playerAngY = player.ang.y;
		playerPos = player.pos;
	}

	const Vector3 lookAt = playerPos + 128._f * gravityMatrix.c1;

	const s16 halfAngX = angX >> 1;
	const s16 halfAngY = angY >> 1;

	const Quaternion rotX = {Cos(halfAngX), Sin(halfAngX)*gravityMatrix.c0};
	const Quaternion rotY = {Cos(halfAngY), Sin(halfAngY)*gravityMatrix.c1};

	res.c2 = (rotY*rotX).RotateSafe(gravityMatrix.c2);
	res.c0 = gravityMatrix.c1.Cross(res.c2).Normalized();
	res.c1 = res.c2.Cross(res.c0);

	Fix12i z = 0xb8'c80_f;

	if (angX > 0x1400)
		z -= 0x0'036_f * (angX - 0x1400);

	res.c3 = lookAt + res.c2 * z;

	return valid && !RUNNING_KUPPA_SCRIPT;
}

// Don't let the vanilla camera rotate the player in the first person state
asm(R"(
nsub_02009c5c:
	push    {r0, r2, r3}
	bl      _ZN7CamCtrl18IsVanillaCamActiveEv
	cmp     r0, #0
	pop     {r0, r2, r3}
	strneh  r3, [r5]
	b       0x02009c60
)");
