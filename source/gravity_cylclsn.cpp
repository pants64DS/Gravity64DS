#include "gravity_actor_extension.h"

template<class T> requires(sizeof(T) == 4) [[gnu::always_inline]]
inline T GetScalarAt(const void* base, std::size_t index)
{
	std::remove_const_t<T> res;

	asm("ldr %[res], [%[base], %[index], lsl #2]" :
		[res]  "=r" (res) :
		[base]  "r" (base),
		[index] "r" (index)
	);

	return res;
}

template<class T> requires(sizeof(T) == 4) [[gnu::always_inline]]
inline void SetScalarAt(void* base, std::size_t index, T val)
{
	asm volatile("str %[val], [%[base], %[index], lsl #2]" ::
		[val]   "r" (val),
		[base]  "r" (base),
		[index] "r" (index)
	);
}

template<FixUR T> struct Fix30 : Fix<T, 30, Fix30> { using Fix<T, 30, Fix30>::Fix; };
using Fix30i = Fix30<int>;

// Assumes that u.Dot(w) > -0.5_f
static void MakeRotationBetween(Matrix3x3& res, const Vector3& u, const Vector3& w)
{
	const Vector3 k = u.Cross(w);

	const Matrix3x3 generator
	{
		0._f, -k.z,  k.y,
		 k.z, 0._f, -k.x,
		-k.y,  k.x, 0._f
	};

	res = generator(generator);

	const Fix30i d = {(Fix12i(Fix30i(1), as_raw) / (1._f + u.Dot(w))).val, as_raw};

	for (int i = 0; i < 9; i++)
	{
		Fix12i a = {d * GetScalarAt<Fix30i>(&res, i), as_raw};

		if (i & 3) // if not on the diagonal
			a += GetScalarAt<Fix12i>(&generator, i);
		else
			a += 1._f;

		SetScalarAt(&res, i, a);
	}
}

// This will be allocated on the stack in the function at 02014aa8
struct CylClsnData
{
	const Actor* fixedCylClsnOwner; // owner of the cylinder collider updated in the outer loop

	const Matrix3x3* movedCylClsnGravity;
	const Matrix3x3* fixedCylClsnGravity;

	Vector3 movedCylClsnPos; // position of the cylinder collider updated in the inner loop
	Matrix3x3 alignRotation; // used to align the cylinder colliders with each other

private:
	static constexpr std::size_t ogAllocSize = 0x14;
	static void Hooks();
};

static_assert(std::is_standard_layout_v<CylClsnData>);

[[gnu::naked, deprecated("This function is not meant to be called!")]]
void CylClsnData::Hooks() { asm volatile (R"(

@ Change the size of the stack allocation
nsub_02014aac:
	sub   r13, r13, %[customAllocSize]
	b     0x02014ab0

nsub_02014abc:
nsub_02014f34:
	addeq r13, r13, %[customAllocSize]
	popeq {r4-r11, r15}
	b     0x02014ac8

@ Get a pointer to the owner of the fixed cylinder collider before entering the inner loop
nsub_02014b08:
	beq   0x02014f14
	bl    _ZN13ActorTreeNode4FindEj
	str   r0, [r13, %[actorPtrOffset]]
	b     0x02014b0c

repl_02014b58: @ replaces a virtual call to CylinderClsn::GetPos
	add    r1,  r13, %[ogAllocSize]
	add    r14, r14, #8
	b      AlignCylColliders

repl_02014f04:
	str    r1, [r8, #0x14]
repl_02014d70:
	mov    r0,  r8
	add    r1,  r13, %[ogAllocSize]
	bl     TransformPushback
	b      0x02014f08
)"
::
	[ogAllocSize]     "I" (ogAllocSize),
	[customAllocSize] "I" (ogAllocSize + sizeof(CylClsnData)),
	[actorPtrOffset]  "I" (ogAllocSize + offsetof(CylClsnData, fixedCylClsnOwner))
);}

// The only calls to GetPos in the updater function are at 0x02014ae0 and 0x02014b60
// The updater doesn't change any of the positions

extern "C" Vector3& AlignCylColliders(CylinderClsn& movedCylClsn, CylClsnData& data)
{
#ifdef GRAVITY_DEBUG_COUNTERS
	extern unsigned cylClsnUpdateCounter;
	++cylClsnUpdateCounter;
#endif

	data.movedCylClsnPos = movedCylClsn.GetPos();

	const Actor* movedCylClsnOwner = ActorTreeNode::Find(movedCylClsn.GetOwnerID());
	const Actor* fixedCylClsnOwner = data.fixedCylClsnOwner;
	
	if (fixedCylClsnOwner && movedCylClsnOwner)
	{
		const Vector3* p1 = &movedCylClsnOwner->pos;
		data.movedCylClsnGravity = &ActorExtension::Get(*movedCylClsnOwner).GetGravityMatrix();

		const Vector3* p2 = &fixedCylClsnOwner->pos;
		data.fixedCylClsnGravity = &ActorExtension::Get(*fixedCylClsnOwner).GetGravityMatrix();

		const Matrix3x3& g1 = *data.movedCylClsnGravity;
		const Matrix3x3& g2 = *data.fixedCylClsnGravity;

		MakeRotationBetween(data.alignRotation, g1.c1, (g1.c1 + g2.c1).Normalized());

		const Matrix3x3& r = data.alignRotation;

		data.movedCylClsnPos = g2.Transpose()(r(r(g1(data.movedCylClsnPos - *p1)) + *p1 - *p2)) + *p2;
	}
	else
		data.movedCylClsnGravity = nullptr;

	return data.movedCylClsnPos;
}

// Transform the pushback vector of the cylinder collider updated in the inner loop
extern "C" void TransformPushback(CylinderClsn& movedCylClsn, CylClsnData& data)
{
	if (data.movedCylClsnGravity)
	{
		const Matrix3x3& g1 = *data.movedCylClsnGravity;
		const Matrix3x3& g2 = *data.fixedCylClsnGravity;

		const auto invR = data.alignRotation.Transpose();

		movedCylClsn.pushback = g1.Transpose()(invR(invR(g2(movedCylClsn.pushback))));
	}
}

// replaces Actor::UpdatePosWithOnlySpeed
Actor& nsub_02010d40(Actor& actor, const CylinderClsn* cylClsn)
{
	auto& extension = ActorExtension::Get(actor);

	Vector3 delta = actor.pos - extension.savedPos + actor.speed;

	if (cylClsn)
	{
		delta.x += cylClsn->pushback.x;
		delta.z += cylClsn->pushback.z;
	}

	delta *= extension.GetGravityMatrix();
	extension.savedPos += delta;
	actor.pos = extension.savedPos;

	return actor;
}
