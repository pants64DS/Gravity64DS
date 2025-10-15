#include "gravity_actor_extension.h"

asm(R"(
@ at the beginning of Actor::Spawn
nsub_02010e34:
	ldr   r5, =spawningActorID
	strh  r0,[r5]
	mov   r5, r0
	b     0x02010e38

@ at the end of Actor::Spawn
nsub_02010e6c:
	ldr   r5, =spawningActorID
	mov   r4, #0
	strh  r4,[r5]
	ldr   r5, =spawningActor
	str   r4,[r5]
	pop   {r4, r5, r15}

_Z18AllocateOnGameHeapj:
	push  {r4, r5, r14}
	b     _ZN9ActorBasenwEj + 4
)");

static_assert(alignof(ActorExtension) <= alignof(Actor));

const Actor* ActorCast(const ActorBase& actorBase)
{
	if (actorBase.category == ActorBase::ACTOR)
		return static_cast<const Actor*>(&actorBase);
	else
		return nullptr;
}

uint16_t spawningActorID = 0;
static std::byte* spawningExtensionAddr;

std::byte* AllocateOnGameHeap(size_t size);

// at the beginning of ActorBase::operator new
void* nsub_02043444(size_t size)
{
	if (spawningActorID == 0)
		return AllocateOnGameHeap(size);

	std::byte* allocAddr = AllocateOnGameHeap(size + sizeof(ActorExtension));

	spawningExtensionAddr = allocAddr + size;
	
	return allocAddr;
}

const Actor* spawningActor = nullptr;

asm(R"(
nsub_020114e0 = _Z18ConstructExtensionR5Actor
nsub_0201162c = _Z18ConstructExtensionR5Actor
)");

Actor& ConstructExtension(Actor& actor)
{
	new (spawningExtensionAddr) ActorExtension(actor);

	spawningActor = &actor;
	return actor;
}

asm(R"(
repl_020112cc:
repl_02011318:
repl_02011378:
	mov  r4, r0
	b    _Z17DestructExtensionRK5Actor
)");

void DestructExtension(const Actor& actor)
{
	ActorExtension::Get(actor).~ActorExtension();
}

ActorExtension& ActorExtension::Get(const Actor& actor)
{
	const std::size_t offset = Memory::gameHeapPtr->Sizeof(&actor) - sizeof(ActorExtension);

	return const_cast<ActorExtension&>(
		*reinterpret_cast<const ActorExtension*>(
			reinterpret_cast<const std::byte*>(&actor) + offset
		)
	);
}

int ActorExtension::CalculateUpVector(Vector3_Q24& __restrict__ res, const Vector3& pos, Sqaerp& sqaerp) const
{
	AssureUnaliased(res) = Vector3_Q24::Raw(currMatrix.c1).NormalizedTwice();

	return sqaerp(res, GetGravityField().GetUpVectorQ24(pos), 1_deg, false, angleToNewField);
}

static Actor& GetHoldingActor(Actor& actor)
{
	if ((actor.flags & Actor::IN_PLAYER_HAND) && PLAYER_ARR[0])
		return *PLAYER_ARR[0];
	else
		return actor;
}

void ActorExtension::UpdateGravity()
{
	Actor& actor = GetActor();

	const Vector3 delta = actor.pos - GetLastUpdatePoint();
	if (delta.Dot(delta) < 1._f && angleToNewField == 0)
		return;

	if (!AlwaysInDefaultField())
	{
		Actor& holdingActor = GetHoldingActor(actor);
		auto& found = GravityField::GetFieldAt(holdingActor.pos);
		const bool fieldChanged = found.GetPriority() >= 0 && &found != &GetGravityField();

		if (fieldChanged)
		{
			GetGravityField().GetActorList().Remove(*this);
			found.GetActorList().Insert(*this);
			SetGravityField(found);

			fieldSqaerp.Reset();
			angleToNewField = 180_deg;
		}

		if (fieldChanged || !GetGravityField().IsHomogeneous() || angleToNewField > 0)
		{
			const Matrix3x3 prevMatrix = currMatrix;

			Vector3_Q24 currUpVector;
			angleToNewField = CalculateUpVector(currUpVector, holdingActor.pos, fieldSqaerp);

			if (IsInTrivialField())
				currMatrix = Matrix3x3::Identity();
			else
			{
				currMatrix.c1 = currUpVector.data.NormalizedTwice();

				if (actor.actorID == 0xbf)
				{
					if (static_cast<const Player&>(actor).currState == &Player::ST_FIRST_PERSON)
					{
						lastUpdatePoint = actor.pos;
						return;
					}

					// This makes the controls of the player feel more accurate and responsive.
					// The difference may not be noticable to an inexperienced player.
					SphericalForwardField (
						currMatrix.c2,
						static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c0),
						static_cast<Vector3_Q24>(INV_VIEW_MATRIX_ASR_3.c2),
						currUpVector
					);
				}
				else
					currMatrix.c2 = currMatrix.c0.Cross(currMatrix.c1);

				currMatrix.c2.NormalizeTwice();
				currMatrix.c0 = currMatrix.c1.Cross(currMatrix.c2).NormalizedTwice();
			}

			ConvertAngle(actor.ang.y, prevMatrix, currMatrix);
			ConvertAngle(actor.motionAng.y, prevMatrix, currMatrix);
		}
	}

	lastUpdatePoint = actor.pos;
}