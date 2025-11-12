#pragma once
#include <functional>
#include "SM64DS_PI.h"

class GravityField;
class ActorExtension;

class ActorList
{
public:
	constexpr ActorList() = default;
	ActorList(const ActorList&) = delete;

	class Node;
	class Range;

	class Node // only to be used as a base class of ActorExtension
	{
	public:
		struct Settings
		{
			bool alwaysInDefaultField = false;
			bool shouldBeTransformed = true;
			bool canSpawnAsSubActor = false;
		};

		friend class ActorList;

		Node(const Node&) = delete;
		Node(Node&&) = delete;
		Node& operator=(const Node&) = delete;
		Node& operator=(Node&&) = delete;

		Settings settings;
		std::reference_wrapper<GravityField> gravityField;
		std::reference_wrapper<Node> next;
		std::reference_wrapper<Node> prev;

	protected:
		Node(Actor& actor);
		~Node();

		constexpr void SetGravityField(GravityField& field) { gravityField = field; }

	public:
		constexpr bool AlwaysInDefaultField() const { return settings.alwaysInDefaultField; }
		constexpr bool ShouldBeTransformed()  const { return settings.shouldBeTransformed; }
		constexpr bool CanSpawnAsSubActor()   const { return settings.canSpawnAsSubActor; }

		constexpr auto OtherNodes() { return Range(*this); }

		constexpr GravityField& GetGravityField() { return gravityField; }
		constexpr const GravityField& GetGravityField() const { return gravityField; }
	};

	static void SetNextSettings(Node::Settings settings);

	class Iterator
	{
		std::reference_wrapper<Node> node;

	public:
		constexpr Iterator(Node& node) : node(node) {}
		constexpr Node& operator* () const { return node; }
		constexpr Node* operator->() const { return &node.get(); }

		constexpr bool operator==(const Iterator& other) const
		{
			return &node.get() == &other.node.get();
		}

		constexpr Iterator& operator++()
		{
			node = node.get().next;
			
			return *this;
		}
	};

	class Range
	{
		Node& node;
	public:
		constexpr explicit Range(Node& node) : node(node) {}
		constexpr Iterator begin() const { return node.next.get(); }
		constexpr Iterator end()   const { return node; }
	};

	void Insert(Node& node);
	void Remove(Node& node);

private:
	Node* last = nullptr;

	Node& GetNextOfNewNode(Node& newNode);
	Node& GetPrevOfNewNode(Node& newNode);
};

asm("_ZN9ActorList15SetNextSettingsENS_4Node8SettingsE = 0x02010e70");
