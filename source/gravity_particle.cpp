#include "gravity_actor_extension.h"

using namespace Particle;

Matrix4x3 particleViewMatrix;
extern Actor* behavingActor;

static void CalculateFieldMatrix(Matrix3x3& res, Vector3_Q24 upAxis)
{
	SphericalMatrixField (
		res,
		static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c0),
		static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c2),
		upAxis
	);
}

static void RotateAroundActor(Vector3& posAsr3, const Actor& actor, const ActorExtension& extension)
{
	posAsr3.RotateAround(actor.pos >> 3, extension.GetGravityMatrix());
}

asm(R"(
repl_02022f04:
	ldr    r1, =particleViewMatrix
	bx     lr

nsub_02049f34:
	mov    r0, r5
	bl     SetParticleViewMatrix
	mov    r0, r4
	b      0x02049f38
)");

extern "C" void SetParticleViewMatrix(const System& system)
{
	const Vector3 pos = system.posAsr3 << 3;
	const GravityField& field = GravityField::GetFieldAt(pos);

	if (field.IsTrivial())
	{
		particleViewMatrix = VIEW_MATRIX_ASR_3;
		return;
	}

	CalculateFieldMatrix(particleViewMatrix.Linear(), field.GetUpVectorQ24(pos));

	particleViewMatrix.c3 = system.posAsr3 - particleViewMatrix.Linear()(system.posAsr3);
	particleViewMatrix = VIEW_MATRIX_ASR_3(particleViewMatrix);
}

asm(R"(
nsub_0204ae2c:
	ldr    r3, =behavingActor
	ldr    r3, [r3]
	cmp    r3, #0
	bne    TransformSystemPos
ContinueTo0204ae30:
	push   {r14}
	b      0x0204ae30
)");

extern "C" void ContinueTo0204ae30(System& system, SysDef* sysDef, const Vector3& posAsr3);

extern "C" void TransformSystemPos(System& system, SysDef* sysDef, const Vector3& posAsr3, const Actor& actor)
{
	const ActorExtension& extension = ActorExtension::Get(actor);

	if (extension.IsInTrivialField())
		return ContinueTo0204ae30(system, sysDef, posAsr3);

	Vector3 newPos = posAsr3;
	RotateAroundActor(newPos, actor, extension);

	return ContinueTo0204ae30(system, sysDef, newPos);
}

asm(R"(
nsub_02022dd8:
	ldr    r0, [r5, #0xc]
	add    r1, r13, #4
	bl     TransformParticles
	mov    r1, r0
	b      0x02022ddc
)");

extern "C" System& TransformParticles(System& system, Vector3& newPosAsr3)
{
	const GravityField* field;

	if (behavingActor)
	{
		const ActorExtension& extension = ActorExtension::Get(*behavingActor);

		if (extension.IsInTrivialField()) return system;
		field = &extension.GetGravityField();

		RotateAroundActor(newPosAsr3, *behavingActor, extension);
	}
	else
	{
		field = &GravityField::GetFieldAt(newPosAsr3 << 3);
		if (field->IsTrivial()) return system;
	}

	Matrix4x3 transform0, transform1;
	CalculateFieldMatrix(transform0.Linear(), field->GetUpVectorQ24(system.posAsr3 << 3));
	CalculateFieldMatrix(transform1.Linear(), field->GetUpVectorQ24(newPosAsr3 << 3));

	transform0.c3 = system.posAsr3 - newPosAsr3 - transform0.Linear()(system.posAsr3);
	transform1.Linear().TransposeInPlace();
	transform1.c3 = newPosAsr3;
	transform1 = transform1(transform0);

	for (auto& particle : system.particleList)
		particle.posAsr3 *= transform1;

	return system;
}
