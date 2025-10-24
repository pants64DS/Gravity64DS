#include "gravity_actor_extension.h"
#include "gravity_field.h"

Actor* behavingActor = nullptr;

void RedirectDelta(Actor& actor, ActorExtension& extension)
{
	if (behavingActor && behavingActor != &actor)
		return;

	if (!extension.ShouldBeTransformed())
		return;

	actor.pos.RotateAround(extension.savedPos, extension.GetGravityMatrix());
	extension.savedPos = actor.pos;
}

static void ProcessBehaviorProperties(Actor& actor, bool beforeBehavior);

// Actor::BeforeBehavior
asm(R"(
nsub_020111e4:
	mov   r0, r4
	bl    _Z14BeforeBehaviorR5Actor
	pop   {r4, r15}
)");

bool BeforeBehavior(Actor& actor)
{
	behavingActor = &actor;

#ifdef GRAVITY_DEBUG_COUNTERS
	extern unsigned activeActorCounter;
	++activeActorCounter;
#endif

	ProcessBehaviorProperties(actor, true);

	return true;
}

// Actor::AfterBehavior
void nsub_02010fc8(Actor& actor, unsigned vfSuccess)
{
	if (behavingActor)
	{
		ProcessBehaviorProperties(actor, false);

		behavingActor = nullptr;
	}

	actor.ActorBase::AfterBehavior(vfSuccess);
}

#ifdef GRAVITY_DEBUG_COUNTERS
extern unsigned behaviorTransformCounter;
#endif

static void ProcessBehaviorProperties(Actor& actor, bool beforeBehavior)
{
	ActorExtension& extension = ActorExtension::Get(actor);

	if (beforeBehavior)
		extension.UpdateGravity();

	static constinit bool shouldTransformOthers;

	if (beforeBehavior)
	{
		shouldTransformOthers = extension.ShouldBeTransformed()
			&& !extension.IsInTrivialField()
			&& !(actor.flags & Actor::IN_PLAYER_HAND);
	}

	if (!shouldTransformOthers) return;

	RedirectDelta(actor, extension);

	if (actor.actorID != 0xbf)
	{
		if (!PLAYER_ARR[0]) return;
		auto& playerExt = ActorExtension::Get(*PLAYER_ARR[0]);

		if (beforeBehavior)
		{
			playerExt.SetProperties(actor, extension);

#ifdef GRAVITY_DEBUG_COUNTERS
			++behaviorTransformCounter;
#endif
		}
		else
			playerExt.RestoreProperties(actor, extension);
	}
	else if (beforeBehavior)
	{
		for (auto& node : extension.OtherNodes()) if (node.ShouldBeTransformed())
		{
			static_cast<ActorExtension&>(node).SetProperties(actor, extension);
#ifdef GRAVITY_DEBUG_COUNTERS
			++behaviorTransformCounter;
#endif
		}
	}
	else
	{
		for (auto& node : extension.OtherNodes()) if (node.ShouldBeTransformed())
			static_cast<ActorExtension&>(node).RestoreProperties(actor, extension);
	}
}

asm(R"(
@ Apply normal acceleration to each actor
nsub_02010cc4:
	push  {r0-r3, r12}
	bl    _Z23ApplyNormalAccelerationRK5Actor
	mov   r4, r0
	pop   {r0-r3, r12}
	b     0x02010cc8
)");

Fix12i ApplyNormalAcceleration(const Actor& actor)
{
	Fix12i res = actor.vertAccel;

	if (actor.actorID == 0xbf)
	{
		auto& player = static_cast<const Player&>(actor);

		if (!player.isInAir || player.wmClsn.IsOnWall())
			return res;
	}

	const auto& extension = ActorExtension::Get(actor);
	const auto& fieldMatrix = extension.GetGravityMatrix();

	const Vector3 v =
		actor.horzSpeed * Sin(actor.motionAng.y) * fieldMatrix.c0 +
		actor.horzSpeed * Cos(actor.motionAng.y) * fieldMatrix.c2;

	const Vector3& pos = extension.GetLastUpdatePoint();

	const Vector3_Q24 u = Vector3_Q24::Raw(fieldMatrix.c1).Normalized();

	Vector3_Q24 w;
	extension.PredictNextUpVector(w, pos + v);

	if (const Fix24i uw = u.Dot(w); uw > 0._f24)
		res += Fix12i(w.Dot(Vector3_Q24::Raw(v)) / uw, as_raw);

	return res;
}

asm(R"(
nsub_02011088:
	mov     r2, #0
	b       0x0201108c

repl_020110ac:
	mov     r2, r4
	b       SetCamSpacePos
)");

extern "C" void SetCamSpacePos(Vector3& v, const Matrix4x3& vietMat, Actor& actor)
{
	v += (actor.rangeOffsetY >> 3) * ActorExtension::Get(actor).GetUpVectorQ12();

	MulVec3Mat4x3(v, vietMat, actor.camSpacePos);
}

// replaces Actor::UpdateCarry
Matrix4x3& nsub_02010180(Actor& actor, Player& player, const Vector3& offset)
{
	auto& extension = ActorExtension::Get(actor);
	const Matrix3x3& playerMatrix = ActorExtension::Get(player).GetGravityMatrix();
	const ModelAnim2& bodyModel = *player.GetBodyModel();
	const Matrix3x3& modelMatrix = bodyModel.mat4x3.Linear();
	const Vector3& bonePos = bodyModel.data.transforms[14].c3;

	actor.pos = playerMatrix(modelMatrix((bonePos << 3) + offset)) + player.pos;
	actor.ang.y = player.ang.y;
	ConvertAngle(actor.ang.y, playerMatrix, extension.GetGravityMatrix());
	actor.motionAng.y = actor.ang.y;

	MATRIX_SCRATCH_PAPER = Matrix4x3::RotationXYZ(actor.ang);
	MATRIX_SCRATCH_PAPER.c3 = actor.pos >> 3;
	extension.savedPos = actor.pos;

	return MATRIX_SCRATCH_PAPER;
}

asm(R"(
nsub_020c4c58_ov_02:
	mov     r0, r4
	pop     {r4, r14}
	b       AfterTalkStateInit
)");

extern "C" bool AfterTalkStateInit(Player& player)
{
	if (behavingActor && behavingActor != &player)
	{
		const Matrix3x3& m = ActorExtension::Get(*behavingActor).GetGravityMatrix();
		player.pos.RotateAround(behavingActor->prevPos, m.Transpose());
	}

	return true;
}

asm(R"(
nsub_020c4e74_ov_02:
	mov     r0, r6
	pop     {r4-r8, r14}
	b       AfterShowMessage2
)");


extern "C" bool AfterShowMessage2(Player& player)
{
	if (behavingActor && behavingActor != &player)
	{
		const ActorExtension& playerExtension = ActorExtension::Get(player);
		if (playerExtension.IsInTrivialField()) return true;

		const Matrix3x3& playerMatrix = playerExtension.GetGravityMatrix();
		const Matrix3x3& actorMatrix = ActorExtension::Get(*behavingActor).GetGravityMatrix();

		player.unk744 = playerExtension.GetRealValue<&Actor::pos>()
			+ playerMatrix.Transpose()(actorMatrix(player.unk744 - player.pos));
	}

	return true;
}
