#include "gravity_cutscene_cam.h"

bool CutsceneCam::CalculateTransform(Matrix4x3& res, Camera& cam, Player& player)
{
	const Matrix3x3& m = ActorExtension::Get(player).GetGravityMatrix();
	res.Linear() = m(cam.camMat.Linear().Transpose());
	res.c3 = m(cam.pos - player.pos) + player.pos;

	return RUNNING_KUPPA_SCRIPT;
}

static void InitCamPosAndLookAt(Vector3& camPos, Vector3& lookAt, const Player& player)
{
	const Vector3 realCamPos = INV_VIEW_MATRIX_ASR_3.c3 << 3;
	const Fix12i dist = realCamPos.Dist(CAMERA->ownerPos);
	const Vector3 realLookAt = realCamPos - INV_VIEW_MATRIX_ASR_3.c2 * dist;
	const Matrix3x3& m = ActorExtension::Get(player).GetGravityMatrix();

	camPos = m.Transpose()(realCamPos - player.pos) + player.pos;
	lookAt = m.Transpose()(realLookAt - player.pos) + player.pos;
}

asm(R"(
ContinueToRunKuppaScript:
	push    {r14}
	b       0x0200ef08
)");

extern "C" bool ContinueToRunKuppaScript(char* address);

bool nsub_0200ef04(char* address)
{
	if (!CamCtrl::IsVanillaCamActive() && PLAYER_ARR[0])
	{
		InitCamPosAndLookAt(CAMERA->pos, CAMERA->lookAt, *PLAYER_ARR[0]);
	}

	return ContinueToRunKuppaScript(address);
}

asm(R"(
repl_020c9dbc_ov_02:
repl_020c81b4_ov_02:
	mov  r1, r4
	b    _Z25MakePlayerLookAtTheCamerasR6Player
)");

void MakePlayerLookAtTheCamera(short camAngleY, Player& player)
{
	if (RUNNING_KUPPA_SCRIPT)
		player.ang.y = camAngleY;
	else
	{
		Vector3 camPos, lookAt;
		InitCamPosAndLookAt(camPos, lookAt, player);
		player.ang.y = lookAt.HorzAngle(camPos);
	}
}
