#include "gravity_actor_list.h"
#include "gravity_field.h"
#include "gravity_actor_extension.h"
#include <optional>

constinit std::optional<ActorList::Node::Settings> nextActorSettings;

// Implements ActorList::SetNextSettings (see nsub_02010e6c)
void nsub_02010e70(ActorList::Node::Settings settings)
{
	nextActorSettings = settings;
}

struct
{
	u16 actorID;
	ActorList::Node::Settings settings;
}
constexpr settingsArray[] = {
	{0x0b4, {0, 0, 0}},
	{0x0fe, {0, 1, 1}},
	{0x121, {0, 0, 0}},
	{0x14a, {1, 0, 0}},
	{0x15d, {0, 0, 0}},
};

static ActorList::Node::Settings GetSettings(const Actor& actor)
{
	if (nextActorSettings)
	{
		const ActorList::Node::Settings res = *nextActorSettings;
		nextActorSettings.reset();
		return res;
	}

	for (auto& entry : settingsArray)
		if (entry.actorID == actor.actorID)
			return entry.settings;

	return {0, 1, 0};
}

[[gnu::target("thumb")]]
ActorList::Node::Node(Actor& actor):
	settings(GetSettings(actor)),
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
