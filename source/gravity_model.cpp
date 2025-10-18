#include "gravity_actor_extension.h"

const ActorBase* renderingActorBase = nullptr;

asm(R"(
nsub_0204322c:
	ldr    r1, =renderingActorBase
	str    r0,[r1]
	push   {r14}
	b      0x02043230

nsub_02043274:
	ldr    r1, =renderingActorBase
	mov    r2, #0
	str    r2,[r1]
	pop    {r15}

@ at the beginning of ModelComponents::Render
nsub_020443c8:
	push   {r4-r11, r14}
	push   {r0, r2, r12}
	mov    r0, r1
	bl     TransformRenderingModel
	mov    r1, r0
	pop    {r0, r2, r12}
	b      0x020443cc
)");

static void TransformModelMatrix(Matrix4x3& modelMatrix, const Matrix3x3& gravityMatrix, const Vector3& pos)
{
	modelMatrix.Linear() = gravityMatrix(modelMatrix.Linear());

	modelMatrix.c3.RotateAround(pos >> 3, gravityMatrix);
}

extern "C" const Matrix4x3* TransformRenderingModel(const Matrix4x3* matrixIn)
{
	if (renderingActorBase == nullptr)
		return matrixIn;

	const Actor* renderingActor = ActorCast(*renderingActorBase);

	if (renderingActor == nullptr)
		return matrixIn;
	
	const auto& extension = ActorExtension::Get(*renderingActor);

	if (extension.IsInTrivialField())
		return matrixIn;

	if (matrixIn == nullptr)
		matrixIn = &Matrix4x3::IDENTITY;

	static constinit Matrix4x3 matrixOut;
	matrixOut = INV_VIEW_MATRIX_ASR_3(*matrixIn);

	TransformModelMatrix(matrixOut, extension.GetGravityMatrix(), renderingActor->pos);

	matrixOut = VIEW_MATRIX_ASR_3(matrixOut);
	return &matrixOut;
}

asm(R"(
repl_02010ba0: @ at the beginning of Actor::DropShadowScaleXYZ
repl_02010bec: @ at the beginning of Actor::DropShadowRadHeight
	ldr    r12, [r0, #0xb0]
	ands   r12, r12, #0x10
	popne  {r15}
	mov    r0, r0, lsl #6
	str    r0, [r1, #0x1c]
	sub    r13, r13, #0xc
	add    r15, r14, #0x14

repl_02015d88: @ in ShadowModel::RenderAll
	mov    r0, r9
	mov    r1, r4
	ldr    r2, [r6, #0x1c]
	mov    r2, r2, lsr #8
	mov    r2, r2, lsl #2
	add    r14, r14, #0x18
	b      TransformRenderingShadow
)");

extern "C" void TransformRenderingShadow(const Matrix4x3& viewMatrix, Matrix4x3& shadowMatrix, const Actor* actor)
{
	if (actor)
	{
		const auto& extension = ActorExtension::Get(*actor);
		const auto& gravityMatrix = extension.GetGravityMatrix();

		if (!extension.IsInTrivialField())
			TransformModelMatrix(shadowMatrix, gravityMatrix, actor->pos);

		// normally the y-coordinate of the translation of the shadow is increased by 2 fxu
		shadowMatrix.c3 += gravityMatrix.c1 << 1;
	}
	else
	{
		const Vector3 pos = shadowMatrix.c3 << 3;

		// normally the y-coordinate of the translation of the shadow is increased by 2 fxu
		shadowMatrix.c3 += GravityField::GetFieldAt(pos).GetUpVectorQ12(pos) << 1;
	}

	shadowMatrix = viewMatrix(shadowMatrix);
}

asm(R"(
repl_0202b94c:
	mov    r0, r1
	add    r14, r14, #0x44
	b      RenderSkyBox
)");

extern "C" void RenderSkyBox(Model& skybox)
{
	Matrix4x3 matrix;
	matrix.Linear() = VIEW_MATRIX_ASR_3.Linear();
	matrix.c3 = {};

	skybox.data.Render(&matrix, nullptr);
}
