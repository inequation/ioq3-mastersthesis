#include "g_local.h"
#include "DependencyGraph.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include <tbb/mutex.h>

using namespace boost;

#define DEP_SHOULD_ASSERT				!NDEBUG
#define DEP_ASSERTS_ARE_FATAL			0

// tricky – when cache is dirty (i.e. deps changed) and we traverse the graph to check if on same island,
// the entity may have been placed in queue on another island and thread, thus it's better to keep this off
#define DEP_TRAVERSE_GRAPH_WHEN_DIRTY	0

// these two are for debugging purposes only – they make everything SLOW!
#define DEP_AUTO_DEPEND					0
#define DEP_AUTO_CACHE_REBUILD			0

#if DEP_SHOULD_ASSERT
	#if DEP_ASSERTS_ARE_FATAL
		#define DEP_ASSERT(Cond, Fmt, ...)	{if (!(Cond)) G_Error("[DepGraph] ERROR: " Fmt "\n", __VA_ARGS__);}
	#else
		#define DEP_ASSERT(Cond, Fmt, ...)	{if (!(Cond)) G_Printf("[DepGraph] WARNING: " Fmt "\n", __VA_ARGS__);}
	#endif
#else
	#define DEP_ASSERT(Cond, Fmt, ...)
#endif

tbb::enumerable_thread_specific<gentity_t *> EntityContext::Context;
size_t gNumOnePlusPopulatedIslands = 0;
size_t gNumOnePopulatedIslands = 0;

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

#if DEP_TRAVERSE_GRAPH_WHEN_DIRTY
		if (Dirty)
		{
			G_Printf("[DepGraph] WARNING: Island cache is dirty! Traversing graph from #%d to #%d, this will be slower!\n", Depends->s.number, On->s.number);

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
#else
		if (Dirty)
			G_Printf("[DepGraph] WARNING: Island cache is dirty! Entities' #%d and #%d co-location may change next frame!\n", Depends->s.number, On->s.number);
#endif
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
		gNumOnePlusPopulatedIslands = i;
		gNumOnePopulatedIslands = 0;
		for (; i < MAX_GENTITIES; ++i)
		{
			level.gentities[i].island = &level.islands[NumIslands++];
			if (level.gentities[i].inuse)
				++gNumOnePopulatedIslands;
		}
		level.num_islands = i;

		Dirty = false;
	}

	static void StallUntilEntityTouched(const gentity_t *ent)
	{
		// FIXME: does this really need to be a busy wait?
		while (ent->frameTouched != level.framenum)
			tbb::this_tbb_thread::yield();
	}

	static void AssertDep(const gentity_t *Ptr)
	{
		if (Ptr)
		{
			auto Context = EntityContext::GetEntity();
			if (Context)
			{
				if (!OnSameIsland(Context, Ptr))
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

	void AutoAddDep(const gentity_t *Ptr)
	{
	#if DEP_AUTO_DEPEND
		if (Ptr)
		{
			auto Context = EntityContext::GetEntity();
			if (Context)
				AddDep(Context, Ptr);
		}
	#endif
	}

	void AutoRemoveDep(const gentity_t *Ptr)
	{
	#if DEP_AUTO_DEPEND
		if (Ptr)
		{
			auto Context = EntityContext::GetEntity();
			if (Context)
				RemoveDep(Context, Ptr);
		}
	#endif
	}
}

// ===========================================================================

EntPtr::EntPtr()
	: Ptr(nullptr)
	, LastCachedFrame(-1)
{

}

EntPtr::EntPtr(gentity_t *entity)
	: Ptr(entity)
	, LastCachedFrame(-1)
{
	DepGraph::AutoAddDep(Ptr);
}

EntPtr::EntPtr(const EntPtr& Other)
	: Ptr(Other.Ptr)
	, LastCachedFrame(Other.LastCachedFrame)
{
	DepGraph::AutoAddDep(Ptr);
}

EntPtr::EntPtr(const WeakEntPtr& Other)
	: Ptr((gentity_t *)Other.Ptr)
	, LastCachedFrame(-1)
{
	DepGraph::AutoAddDep(Ptr);
}

EntPtr::~EntPtr()
{
	DepGraph::AutoRemoveDep(Ptr);
}

EntPtr& EntPtr::operator=(const EntPtr& Other)
{
	DepGraph::AutoRemoveDep(Ptr);
	Ptr = Other.Ptr;
	LastCachedFrame = Other.LastCachedFrame;
	DepGraph::AutoAddDep(Ptr);
	return *this;
}

EntPtr& EntPtr::operator=(const WeakEntPtr& Other)
{
	DepGraph::AutoRemoveDep(Ptr);
	Ptr = (gentity_t *)Other.Ptr;
	LastCachedFrame = -1;
	DepGraph::AutoAddDep(Ptr);
	return *this;
}

EntPtr& EntPtr::operator=(gentity_t *entity)
{
	DepGraph::AutoRemoveDep(Ptr);
	Ptr = entity;
	LastCachedFrame = -1;
	DepGraph::AutoAddDep(Ptr);
	return *this;
}

gentity_t& EntPtr::operator*() const
{
	CachedAssertDep();
	return *Ptr;
}

gentity_t *EntPtr::operator->() const
{
	CachedAssertDep();
	return Ptr;
}

EntPtr::operator gentity_t *() const
{
	CachedAssertDep();
	return Ptr;
}

EntPtr& EntPtr::operator++()
{
	DepGraph::AutoRemoveDep(Ptr);
	++Ptr;
	LastCachedFrame = -1;
	DepGraph::AutoAddDep(Ptr);
	CachedAssertDep();
	return *this;
}

EntPtr EntPtr::operator++(int)
{
	DepGraph::AutoRemoveDep(Ptr);
	auto RetVal = Ptr++;
	LastCachedFrame = -1;
	DepGraph::AutoAddDep(Ptr);
	CachedAssertDep();
	return RetVal;
}

void EntPtr::CachedAssertDep() const
{
	// if we've already asserted dependency on this frame, do nothing
	if (LastCachedFrame != level.framenum)
	{
		LastCachedFrame = level.framenum;
		DepGraph::AssertDep(Ptr);
	}
}

// ===========================================================================

WeakEntPtr::WeakEntPtr()
	: Ptr(nullptr)
{

}

WeakEntPtr::WeakEntPtr(const gentity_t *entity)
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

WeakEntPtr& WeakEntPtr::operator=(const WeakEntPtr& Other)
{
	Ptr = Other.Ptr;
	return *this;
}

WeakEntPtr& WeakEntPtr::operator=(const EntPtr& Other)
{
	Ptr = Other.Ptr;
	return *this;
}

WeakEntPtr& WeakEntPtr::operator=(const gentity_t *entity)
{
	Ptr = entity;
	return *this;
}

const gentity_t& WeakEntPtr::operator*() const
{
	return *Ptr;
}

const gentity_t *WeakEntPtr::operator->() const
{
	return Ptr;
}

WeakEntPtr::operator const gentity_t *() const
{
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

// ===========================================================================

tbb::task *ProcessIslandsTask::execute()
{
	class PerIslandTask : public tbb::task
	{
	public:
		PerIslandTask(EntityIsland *InIsland)
			: Island(InIsland)
		{}

		tbb::task *execute()
		{
			((ProcessIslandsTask *)parent())->Func(Island);
			return nullptr;
		}
	private:
		EntityIsland *Island;
	};

	set_ref_count(level.num_islands + 1);
	for (int i = 0; i < level.num_islands; ++i)
	{
		auto& task = *new(allocate_child()) PerIslandTask(&level.islands[i]);
		if (i < level.num_islands - 1)
			spawn(task);
		else
			spawn_and_wait_for_all(task);
	}
	return nullptr;
}

void ProcessIslandsTask::Run(PerIslandFunc PerIsland)
{
	auto& task = *new(tbb::task::allocate_root()) ProcessIslandsTask(PerIsland);
	tbb::task::spawn_root_and_wait(task);
}
