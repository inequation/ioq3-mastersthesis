#include "g_local.h"
#include "DependencyGraph.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/breadth_first_search.hpp>

using namespace boost;

#define DEP_SHOULD_ASSERT		!_NDEBUG
#define DEP_ASSERTS_ARE_FATAL	1
#define DEP_AUTO_DEPEND			0
#define DEP_AUTO_CACHE_REBUILD	0

#if DEP_ASSERTS_ARE_FATAL
	#define DEP_ASSERT(Cond, Fmt, ...)	{if (!Cond) {fprintf(stderr, Fmt, __VA_ARGS__); abort();}}
#else
	#define DEP_ASSERT(Cond, Fmt, ...)	{if (!Cond) G_Printf("[DepGraph] WARNING: " Fmt "\n", __VA_ARGS__);}
#endif

tbb::enumerable_thread_specific<gentity_t *> EntityContext::Context;

// ===========================================================================

namespace DepGraph
{
	typedef adjacency_list<vecS, vecS, undirectedS>	GraphType;
	GraphType Graph;

	static bool Dirty = true;

	void AddDep(gentity_t *Depends, gentity_t *On)
	{
		if (Depends == On)
			return;

		add_edge(Depends->s.number, On->s.number, Graph);

		Dirty = true;

#if DEP_AUTO_CACHE_REBUILD
		RebuildIslands();
#endif
	}

	void RemoveDep(gentity_t *Depends, gentity_t *On)
	{
		if (Depends == On)
			return;

		add_edge(Depends->s.number, On->s.number, Graph);

		Dirty = true;

#if DEP_AUTO_CACHE_REBUILD
		RebuildIslands();
#endif
	}

	bool IsDependent(gentity_t *Depends, gentity_t *On)
	{
		if (Depends == On)
			return true;

		bool SameIsland = false;

#if DEP_AUTO_CACHE_REBUILD
		if (Dirty)
			RebuildIslands();
#endif

		if (Dirty)
		{
			DEP_ASSERT(0, "Dependency graph is dirty! Traversing graph from #%d to #%d, this will be slower!", Depends->s.number, On->s.number);

#ifdef __EXCEPTIONS
			class FoundException
			{};
#endif

			class Pathfinder : public default_bfs_visitor
			{
			public:
				typedef graph_traits<GraphType>::vertex_descriptor Vertex;

				Pathfinder(Vertex InTarget, bool& InFlag)
					: Target(InTarget), Flag(InFlag)
				{}

				void discover_vertex(Vertex Vert, const GraphType& G) const
				{
					if (Vert == Target)
					{
						Flag = true;
#ifdef __EXCEPTIONS
						throw FoundException();
#endif
					}
				}
			private:
				Vertex Target;
				bool& Flag;
			};

#ifdef __EXCEPTIONS
			try {
#endif
			breadth_first_search(Graph, vertex(Depends->s.number, Graph), visitor(Pathfinder(On->s.number, SameIsland)));
#ifdef __EXCEPTIONS
			} catch (FoundException Ex) {
				// deliberate no-op, this is just to stop the search
				(void)Ex;
			}
#endif
		}
		else
		{
			auto Island = Depends->island;
			SameIsland = std::find(Island->begin(), Island->end(), On) != Island->end();
		}

		DEP_ASSERT(SameIsland, "Entity #%d is on a different island than the dependency #%d!", Depends->s.number, On->s.number);
		return SameIsland;
	}

	void RebuildIslands()
	{
		if (!Dirty)
			return;

		for (int i = 0; i < level.num_islands; ++i)
			level.islands[i].clear();

		typedef std::vector<int> IntVector;
		IntVector IslandMap(num_vertices(Graph));

		auto NumIslands = connected_components(Graph, &IslandMap[0]);
		IntVector::size_type i;
		for (i = 0; i < IslandMap.size(); ++i)
		{
			level.gentities[i].island = &level.islands[IslandMap[i]];
			level.gentities[i].island->push_back(&level.gentities[i]);
		}
		for (; i < MAX_GENTITIES; ++i)
		{
			level.gentities[i].island = &level.islands[NumIslands++];
		}
		level.num_islands = i;

		Dirty = false;
	}
}

// ===========================================================================

EntPtr::EntPtr()
	: Ptr(nullptr)
{

}

EntPtr::EntPtr(gentity_t *entity)
	: Ptr(entity)
{
	AutoAddDep();
}

EntPtr::EntPtr(const EntPtr& Other)
	: Ptr(Other.Ptr)
{
	AutoAddDep();
}

EntPtr::EntPtr(const WeakEntPtr& Other)
	: Ptr(Other.Ptr)
{
	AutoAddDep();
}

EntPtr::~EntPtr()
{
	AutoRemoveDep();
}

EntPtr& EntPtr::operator=(gentity_t *entity)
{
	AutoRemoveDep();
	Ptr = entity;
	AutoAddDep();
	return *this;
}

gentity_t& EntPtr::operator*() const
{
	AssertDep();
	return *Ptr;
}

gentity_t *EntPtr::operator->() const
{
	AssertDep();
	return Ptr;
}

EntPtr::operator gentity_t *() const
{
	AssertDep();
	return Ptr;
}

EntPtr& EntPtr::operator++()
{
	AutoRemoveDep();
	++Ptr;
	AutoAddDep();
	return *this;
}

EntPtr EntPtr::operator++(int)
{
	AutoRemoveDep();
	auto RetVal = Ptr++;
	AutoAddDep();
	return RetVal;
}

void EntPtr::AssertDep() const
{
#if DEP_SHOULD_ASSERT
	if (Ptr)
	{
		auto Context = EntityContext::GetEntity();
		if (Context)
		{
			DEP_ASSERT(!DepGraph::Dirty, "Dependency cache dirty while trying to assert dependency of #%d on #%d!", Context->s.number, Ptr->s.number);
			DEP_ASSERT(DepGraph::IsDependent(Context, Ptr), "Entity #%d has not declared dependency on #%d! Data race may occur!", Context->s.number, Ptr->s.number);
		}
	}
#endif
}

void EntPtr::AutoAddDep() const
{
#if DEP_AUTO_DEPEND
	if (Ptr)
	{
		auto Context = EntityContext::GetEntity();
		if (Context)
			DepGraph::AddDep(Context, Ptr);
	}
#endif
}

void EntPtr::AutoRemoveDep() const
{
#if DEP_AUTO_DEPEND
	if (Ptr)
	{
		auto Context = EntityContext::GetEntity();
		if (Context)
			DepGraph::RemoveDep(Context, Ptr);
	}
#endif
}

// ===========================================================================

WeakEntPtr::WeakEntPtr()
	: Ptr(nullptr)
{

}

WeakEntPtr::WeakEntPtr(gentity_t *entity)
	: Ptr(entity)
{

}

WeakEntPtr::WeakEntPtr(const WeakEntPtr& Other)
	: Ptr(Other.Ptr)
{

}

WeakEntPtr::WeakEntPtr(const EntPtr& Other)
	: Ptr(Other.Ptr)
{

}

WeakEntPtr::~WeakEntPtr()
{

}

WeakEntPtr& WeakEntPtr::operator=(gentity_t *entity)
{
	Ptr = entity;
	return *this;
}

const gentity_t& WeakEntPtr::operator*() const
{
	AssertDep();
	return *Ptr;
}

const gentity_t *WeakEntPtr::operator->() const
{
	AssertDep();
	return Ptr;
}

WeakEntPtr::operator const gentity_t *() const
{
	AssertDep();
	return Ptr;
}

WeakEntPtr& WeakEntPtr::operator++()
{
	++Ptr;
	return *this;
}

WeakEntPtr WeakEntPtr::operator++(int)
{
	auto RetVal = Ptr++;
	return RetVal;
}

void WeakEntPtr::AssertDep() const
{
#if DEP_SHOULD_ASSERT
	if (Ptr)
	{
		auto Context = EntityContext::GetEntity();
		if (Context && !DepGraph::IsDependent(Context, Ptr))
			G_Printf("[DepGraph] WARNING: Dependent entity #%d is not on the same island as #%d! Data might be out of date!\n", Context->s.number, Ptr->s.number);
	}
#endif
}
