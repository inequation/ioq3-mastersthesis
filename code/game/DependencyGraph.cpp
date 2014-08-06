#include "g_local.h"
#include "DependencyGraph.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include <tbb/mutex.h>

using namespace boost;

#define DEP_SHOULD_ASSERT		!NDEBUG
#define DEP_ASSERTS_ARE_FATAL	0

// these two are for debugging purposes only – they make everything SLOW!
#define DEP_AUTO_DEPEND			0
#define DEP_AUTO_CACHE_REBUILD	0

#if DEP_SHOULD_ASSERT
	#if DEP_ASSERTS_ARE_FATAL
		#define DEP_ASSERT(Cond, Fmt, ...)	{if (!(Cond)) G_Error("[DepGraph] ERROR: " Fmt "\n", __VA_ARGS__);}
	#else
		#define DEP_ASSERT(Cond, Fmt, ...)	{if (!(Cond)) G_Printf("[DepGraph] WARNING: " Fmt "\n", __VA_ARGS__);}
	#endif
#endif

tbb::enumerable_thread_specific<gentity_t *> EntityContext::Context;

// ===========================================================================

namespace DepGraph
{
	typedef adjacency_list<vecS, vecS, undirectedS>	GraphType;
	GraphType Graph;
	tbb::mutex GraphMutex;

	static volatile bool Dirty = true;

	void AddDep(const gentity_t *Depends, const gentity_t *On)
	{
		if (Depends == On)
			return;

		DEP_ASSERT(On != nullptr, "Trying to add a null dependency to #%d! Need to remove/toggle instead?", Depends->s.number);

		{
			tbb::mutex::scoped_lock lock(GraphMutex);
			add_edge(Depends->s.number, On->s.number, Graph);
			Dirty = true;
		}

#if DEP_AUTO_CACHE_REBUILD
		RebuildIslands();
#endif
	}

	void RemoveDep(const gentity_t *Depends, const gentity_t *On)
	{
		if (Depends == On || On == nullptr)
			return;

		{
			tbb::mutex::scoped_lock lock(GraphMutex);
			remove_edge(Depends->s.number, On->s.number, Graph);
			Dirty = true;
		}

#if DEP_AUTO_CACHE_REBUILD
		RebuildIslands();
#endif
	}

	void RemoveVertex(const gentity_t *Vertex)
	{
		if (Vertex == nullptr)
			return;

		{
			tbb::mutex::scoped_lock lock(GraphMutex);
			for (int i = 0; i < MAX_GENTITIES; ++i)
			{
				if (edge(Vertex->s.number, i, Graph).second)
					remove_edge(Vertex->s.number, i, Graph);
			}
			Dirty = true;
		}

#if DEP_AUTO_CACHE_REBUILD
		RebuildIslands();
#endif
	}

	bool OnSameIsland(const gentity_t *Depends, const gentity_t *On)
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
			G_Printf("[DepGraph] WARNING: Dependency graph is dirty! Traversing graph from #%d to #%d, this will be slower!\n", Depends->s.number, On->s.number);

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
			// make sure the graph includes the depending vertex
			while (num_vertices(Graph) <= Depends->s.number)
				add_vertex(Graph);
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
			SameIsland = std::binary_search(Island->begin(), Island->end(), On);
		}

		return SameIsland;
	}

	void RebuildIslands()
	{
		if (!Dirty)
			return;

		tbb::mutex::scoped_lock lock(GraphMutex);

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

static void StallUntilEntityTouched(gentity_t *ent)
{
	// FIXME: does this really need to be a busy wait?
	while (ent->frameTouched != level.framenum)
		tbb::this_tbb_thread::yield();
}

static void AssertDep(gentity_t *Ptr)
{
	if (Ptr)
	{
		auto Context = EntityContext::GetEntity();
		if (Context)
		{
			if (!DepGraph::OnSameIsland(Context, Ptr))
			{
				// oops, sync needed – decide based on entity numbers (i.e. pointer addresses)
				if (Ptr < Context)
				{
					// requested entity would be processed before context in the single-thread implementation, so wait for it
					DEP_ASSERT(false, "STALL! Entity #%d is not on the same island as #%d!", Context->s.number, Ptr->s.number);
					StallUntilEntityTouched(Ptr);
				}
#if DEP_SHOULD_ASSERT
				else
				{
					// requested entity would be processed after context, we *should* be fine with accessing it (data races may still occur!), as single-thread impl. would simply use previous frame's data
					G_Printf("[DepGraph] WARNING: RACE DANGER! Entity #%d is not on the same island as #%d!\n", Context->s.number, Ptr->s.number);
				}
#endif
			}
		}
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
	AssertDep(Ptr);
	return *Ptr;
}

gentity_t *EntPtr::operator->() const
{
	AssertDep(Ptr);
	return Ptr;
}

EntPtr::operator gentity_t *() const
{
	AssertDep(Ptr);
	return Ptr;
}

EntPtr& EntPtr::operator++()
{
	AutoRemoveDep();
	++Ptr;
	AutoAddDep();
	AssertDep(Ptr);
	return *this;
}

EntPtr EntPtr::operator++(int)
{
	AutoRemoveDep();
	auto RetVal = Ptr++;
	AutoAddDep();
	AssertDep(Ptr);
	return RetVal;
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
