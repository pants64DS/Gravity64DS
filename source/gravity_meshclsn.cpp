#include "gravity_actor_extension.h"

extern Actor* behavingActor;
extern const Actor* spawningActor;

void RedirectDelta(Actor& actor, ActorExtension& extension);

class Transform
{
public:
	using Func = void(BgCh&, const Vector3&, const Matrix3x3&, bool afterClsn);

private:
	ActorExtension* extension = nullptr;
	BgCh* bgch = nullptr;
	Func* func = nullptr;

public:
	void operator()(const Vector3& v, const Matrix3x3& m) const
	{
		func(*bgch, v, m, false);
	}

	void operator()(const Vector3& v, Matrix3x3::TransposeProxy m) const
	{
		func(*bgch, v, m.Transpose(), true);
	}

	void Activate(ActorExtension& extension, BgCh& bgch, Func& func)
	{
		this->extension = &extension;
		this->bgch = &bgch;
		this->func = &func;
	}

	void Deactivate() { extension = nullptr; }
	ActorExtension* GetActorExtension() { return extension; }
	constexpr bool IsActive() const { return extension; }
	consteval Transform() = default;
}
static constinit transform;

template<class T, std::invocable<T&, const Vector3&, const Matrix3x3&, bool> F>
static void ApplyBgChTransform(T& bgch, F&&)
{
	[](BgCh& bgch, Transform::Func& func) // a lambda to avoid code duplication
	{
		if (Actor* actor = bgch.objPtr ?: behavingActor)
		{
#ifdef GRAVITY_DEBUG_COUNTERS
			extern unsigned bgChTransformCounter;
			++bgChTransformCounter;
#endif

			auto& extension = ActorExtension::Get(*actor);
			if (extension.IsInTrivialField()) return;

			transform.Activate(extension, bgch, func);
			transform(extension.savedPos, extension.GetGravityMatrix());
		}
	}
	(bgch, *[](BgCh& bgch, const Vector3& actorPos, const Matrix3x3& m, bool afterClsn)
	{
		if (afterClsn)
			F()(static_cast<T&>(bgch), actorPos, m.Transpose(), afterClsn);
		else
			F()(static_cast<T&>(bgch), actorPos, m, afterClsn);
	});
}

static void RestoreBgChTransform()
{
	if (!transform.IsActive()) return;
	
	auto* extension = transform.GetActorExtension();

	transform(extension->GetActor().pos, extension->GetGravityMatrix().Transpose());
	transform.Deactivate();
}

asm(R"(
repl_0203863c: @ the beginning of RaycastLine::DetectClsn
	sub   r13, r13, #0x1c
	b     _Z10BeforeClsnR11RaycastLine

repl_020385a0: @ the beginning of RaycastLine::DetectClsnStageOnly
	sub   r13, r13, #8
	b     _Z10BeforeClsnR11RaycastLine
)");

constinit const RaycastGround* currRaycastGround = nullptr;
constinit const GravityField* raycastGroundField = nullptr;

RaycastLine& BeforeClsn(RaycastLine& line)
{
	ApplyBgChTransform(line, [](RaycastLine& line, const Vector3& actorPos, const auto& rotation, bool afterClsn)
	{
		line.line.pos0.RotateAround(actorPos, rotation);

		if (currRaycastGround && !afterClsn)
		{
			const Vector3& p0 = line.line.pos0;
			Vector3 v;

			if (const Vector3* u = raycastGroundField->GetHomogeneousUpVectorQ12())
				v = *u << 10;
			else
			{
				v = raycastGroundField->GetAltitudeVector(p0);

				if (const Fix12i altitude = v.Len(); altitude > 1024._f)
					v *= 1024._f / altitude;
			}

			line.SetObjAndLine(p0, p0 - v, line.objPtr);
			line.result = currRaycastGround->result;
		}
		else
		{
			line.line.pos1.RotateAround(actorPos, rotation);
			line.average.RotateAround(actorPos, rotation);
			line.clsnPos.RotateAround(actorPos, rotation);
		}
	});

	return line;
}

asm(R"(
repl_02038b7c: @ the beginning of SphereClsn::DetectClsn
	sub   r13, r13, #0x1c
	b     _Z10BeforeClsnR10SphereClsn
)");

SphereClsn& BeforeClsn(SphereClsn& sphere)
{
	ApplyBgChTransform(sphere, [](SphereClsn& sphere, const Vector3& actorPos, const auto& rotation, bool afterClsn)
	{
		sphere.pos.RotateAround(actorPos, rotation);
	});

	return sphere;
}

asm("nsub_0203881c = _Z9AfterClsnb"); // the end of RayCastLine::DetectClsn
asm("nsub_02038630 = _Z9AfterClsnb"); // the end of RayCastLine::DetectClsnStageOnly
asm("nsub_02038e9c = _Z9AfterClsnb"); // the end of SphereClsn::DetectClsn

bool AfterClsn(bool res)
{
	RestoreBgChTransform();

	return res;
}

void TransformNormal(Vector3& normal)
{
	if (auto* extension = transform.GetActorExtension())
	{
		normal *= extension->GetGravityMatrix().Transpose();
		normal.NormalizeTwice();
	}
}

void nsub_01ffd91c(void*, void*, Vector3& normal)
{
	TransformNormal(normal);
}

void repl_02039f10(const MovingMeshCollider& meshClsn, const Vector3& normal, Vector3& res)
{
	res = meshClsn.newTransform.Linear()(normal);

	TransformNormal(res);
}

asm(R"(
_Z20ContinueToDetectClsnR13RaycastGround:
	push    {r4-r11, r14}
	sub     r13, r13, #0x1c
	b       0x02038f4c

nsub_02038f48 = _Z15TransformNormalR7Vector3
)");

bool ContinueToDetectClsn(RaycastGround&);

bool nsub_02038f44(RaycastGround& raycastGround)
{
	const Vector3& pos0 = raycastGround.pos;
	const GravityField* field;
	Actor* actor = raycastGround.objPtr ?: behavingActor;

	if (actor)
	{
		const ActorExtension& extension = ActorExtension::Get(*actor);

		if (extension.IsInTrivialField())
			return ContinueToDetectClsn(raycastGround);

		field = &extension.GetGravityField();
	}
	else
	{
		field = &GravityField::GetFieldAt(pos0);

		if (field->IsTrivial())
			return ContinueToDetectClsn(raycastGround);
	}

	currRaycastGround = &raycastGround;
	raycastGroundField = field;

	RaycastLine line;
	line.line.pos0 = pos0;
	line.objPtr = actor;
	line.flags = raycastGround.flags;

	const bool detected = line.DetectClsn();
	currRaycastGround = nullptr;
	raycastGroundField = nullptr;

	raycastGround.flags = line.flags;
	raycastGround.result = line.result;
	raycastGround.clsnPosY = pos0.y - line.line.pos0.Dist(line.clsnPos);
	raycastGround.hadCollision = line.hadCollision;

	return detected;
}

static const Matrix4x3& TransformMMC(MovingMeshCollider& meshClsn, const Matrix4x3& transform, Matrix4x3& res)
{
	const Actor* actor = meshClsn.actor;

	if (actor == nullptr)
	{
		actor = spawningActor;

		if (actor == nullptr)
			actor = behavingActor;
	}

	if (actor)
	{
		const auto& extension = ActorExtension::Get(*actor);

		if (!extension.IsInTrivialField())
		{
			res.Linear() = extension.GetGravityMatrix()(transform.Linear());
			res.c3 = transform.c3;

			return res;
		}
	}

	return transform;
}

asm(R"(
_Z19ContinueToTransformR18MovingMeshColliderRK9Matrix4x3s:
	push  {r4-r11, r14}
	b     0x02039f24

_Z17ContinueToSetFileR18MovingMeshColliderPcRK9Matrix4x35Fix12IiEsR10CLPS_Block:
	push  {r4-r10, r14}
	b     0x0203a1e4
)");

void ContinueToTransform(MovingMeshCollider& meshClsn, const Matrix4x3& newTransform, short angleY);

void ContinueToSetFile (
	MovingMeshCollider& meshClsn, char* clsnFile,
	const Matrix4x3& transform, Fix12i scale,
	short angleY, CLPS_Block& clps
);

void nsub_0203a1e0 (
	MovingMeshCollider& meshClsn, char* clsnFile,
	const Matrix4x3& transform, Fix12i scale,
	short angleY, CLPS_Block& clps
)
{
	Matrix4x3 res;
	const Matrix4x3& newTransform = TransformMMC(meshClsn, transform, res);
	ContinueToSetFile(meshClsn, clsnFile, newTransform, scale, angleY, clps);
}

void nsub_02039f20(MovingMeshCollider& meshClsn, const Matrix4x3& transform, short angleY)
{
	Matrix4x3 res;
	const Matrix4x3& newTransform = TransformMMC(meshClsn, transform, res);
	ContinueToTransform(meshClsn, newTransform, angleY);
}

auto repl_020383a4() // before calling the function that calls beforeClsnCallback
{
	if (behavingActor)
	{
		auto& extension = ActorExtension::Get(*behavingActor);

		if (!extension.IsInTrivialField())
			RedirectDelta(*behavingActor, extension);
	}

	return ACTIVE_MESH_COLLIDERS;
}

void nsub_020383dc() // after calling the function that calls beforeClsnCallback
{
	if (behavingActor)
		ActorExtension::Get(*behavingActor).savedPos = behavingActor->pos;
}

Vector3 savedPrevPos;

asm(R"(
repl_0203702c: @ at the beginning of WithMeshClsn::UpdateDiscreteNoLava
repl_02036eec: @ at the beginning of WithMeshClsn::UpdateDiscreteNoLava_2
	mov   r7, r0
	b    _Z15BeforeWMCUpdateRK12WithMeshClsn

repl_020366bc: @ at the beginning of WithMeshClsn::UpdateContinuous
repl_02036320: @ at the beginning of WithMeshClsn::UpdateContinuousNoLava
repl_020358b4: @ at the beginning of WithMeshClsn::UpdateExtraContinuous
	mov   r10, r0
	b    _Z15BeforeWMCUpdateRK12WithMeshClsn
)");

const WithMeshClsn& BeforeWMCUpdate(const WithMeshClsn& wmClsn)
{
	if (!wmClsn.actor) return wmClsn;
	
	Actor& actor = *wmClsn.actor;
	auto& extension = ActorExtension::Get(actor);

	const bool trivial = extension.IsInTrivialField();

	if (!trivial)
		RedirectDelta(actor, extension);

	savedPrevPos = actor.prevPos;

	if (!trivial)
		actor.prevPos.RotateAround(actor.pos, extension.GetGravityMatrix().Transpose());

	return wmClsn;
}

asm(R"(
repl_0203717c: @ at the first exit point of WithMeshClsn::UpdateDiscreteNoLava
repl_02036ff0: @ at the first exit point of WithMeshClsn::UpdateDiscreteNoLava_2
	bxne   lr
	b      nsub_020371a8
repl_02037194: @ at the second exit point of WithMeshClsn::UpdateDiscreteNoLava
repl_02037008: @ at the second exit point of WithMeshClsn::UpdateDiscreteNoLava_2
	bxeq   lr
nsub_020371a8: @ at the last exit point of WithMeshClsn::UpdateDiscreteNoLava
nsub_0203701c: @ at the last exit point of WithMeshClsn::UpdateDiscreteNoLava_2
	mov   r0, r7
	pop   {r4-r7, r14}
	b     _Z14AfterWMCUpdateRK12WithMeshClsn

nsub_02036ac4: @ at the only exit point of WithMeshClsn::UpdateContinuous
nsub_020366ac: @ at the only exit point of WithMeshClsn::UpdateContinuousNoLava
nsub_0203630c: @ at the only exit point of WithMeshClsn::UpdateExtraContinuous
	mov   r0, r10
	pop   {r4-r11, r14}
	b     _Z14AfterWMCUpdateRK12WithMeshClsn
)");

void AfterWMCUpdate(const WithMeshClsn& wmClsn)
{
	if (wmClsn.actor)
	{
		auto& extension = ActorExtension::Get(*wmClsn.actor);

		if (!extension.IsInTrivialField())
			RedirectDelta(*wmClsn.actor, extension);

		wmClsn.actor->prevPos = savedPrevPos;
	}
}
