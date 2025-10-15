#include "gravity_actor_list.h"
#include "gravity_field.h"
#include "gravity_actor_extension.h"

template<auto... keys>
constexpr bool In(auto key) { return (... || (key == keys)); }

[[gnu::target("thumb")]]
ActorList::Node::Node(Actor& actor):
	alwaysInDefaultField(In<
		0x14a  // number
	>(actor.actorID)),

	shouldBeTransformed(!In<
		0xb4,  // star marker
		0x121, // red coin
		0x14a, // number
		0x15d, // exit warp
		0x233  // pseudo-mesh sphere
	>(actor.actorID)),
	canSpawnAsSubActor(In<
		0xfe   // piranha plant flame
	>(actor.actorID)),

	gravityField(GravityField::GetFieldFor(actor, *this)),

	next(gravityField.get().GetActorList().GetNextOfNewNode(*this)),
	prev(gravityField.get().GetActorList().GetPrevOfNewNode(*this))
{
	gravityField.get().GetActorList().last = this;
}

[[gnu::target("thumb")]]
ActorList::Node::~Node()
{
	gravityField.get().GetActorList().Remove(*this);
}

[[gnu::target("thumb")]]
ActorList::Node& ActorList::GetNextOfNewNode(Node& newNode)
{
	if (last == nullptr) return newNode;

	Node& first = last->next.get();
	first.prev = newNode;
	return first;
}

[[gnu::target("thumb")]]
ActorList::Node& ActorList::GetPrevOfNewNode(Node& newNode)
{
	if (last == nullptr) return newNode;

	last->next = newNode;
	return *last;
}

[[gnu::target("thumb")]]
void ActorList::Insert(Node& node)
{
	node.next = GetNextOfNewNode(node);
	node.prev = GetPrevOfNewNode(node);
	last = &node;
}

[[gnu::target("thumb")]]
void ActorList::Remove(Node& node)
{
	if (&node == last)
	{
		Node* secondLast = &last->prev.get();

		if (last == secondLast)
			last = nullptr;
		else
			last = secondLast;
	}

	node.prev.get().next = node.next;
	node.next.get().prev = node.prev;
}
