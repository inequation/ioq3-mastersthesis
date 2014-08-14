#pragma once

#include <tbb/task.h>
#include <tbb/enumerable_thread_specific.h>
#include <vector>
#include <functional>

struct WeakEntPtr;

// basic, update order-enforcing entity smart pointer
struct EntPtr
{
	EntPtr();
	EntPtr(gentity_t *entity);
	EntPtr(const EntPtr& Other);
	EntPtr(const WeakEntPtr& Other);
	~EntPtr();

	EntPtr& operator=(const EntPtr& Other);
	EntPtr& operator=(const WeakEntPtr& Other);
	EntPtr& operator=(gentity_t *entity);

	gentity_t& operator*() const;
	gentity_t *operator->() const;
	operator gentity_t *() const;
	EntPtr& operator++();
	EntPtr operator++(int);

	gentity_t *GetPtrNoCheck() const
	{return Ptr;}
	bool isNull() const
	{return Ptr == nullptr;}

	bool operator==(const EntPtr& Other) const
	{return Ptr == Other.Ptr;}
	bool operator!=(const EntPtr& Other) const
	{return Ptr != Other.Ptr;}
	bool operator==(const gentity_t *entity) const
	{return Ptr == entity;}
	bool operator!=(const gentity_t *entity) const
	{return Ptr != entity;}

private:
	gentity_t	*Ptr;
	mutable int	LastCachedFrame;

	void CachedAssertDep() const;

	friend struct ScopedEntityContext;
	friend struct WeakEntPtr;
};

// ===========================================================================

// non-update order-enforcing, read-only entity smart pointer
struct WeakEntPtr
{
	WeakEntPtr();
	WeakEntPtr(const gentity_t *entity);
	WeakEntPtr(const EntPtr& Other);
	WeakEntPtr(const WeakEntPtr& Other);
	~WeakEntPtr();

	WeakEntPtr& operator=(const EntPtr& Other);
	WeakEntPtr& operator=(const WeakEntPtr& Other);
	WeakEntPtr& operator=(const gentity_t *entity);

	const gentity_t& operator*() const;
	const gentity_t *operator->() const;
	operator const gentity_t *() const;
	WeakEntPtr& operator++();
	WeakEntPtr operator++(int);

	const gentity_t *GetPtrNoCheck() const
	{return Ptr;}
	bool isNull() const
	{return Ptr == nullptr;}

	bool operator==(const EntPtr& Other) const
	{return Ptr == Other.Ptr;}
	bool operator!=(const EntPtr& Other) const
	{return Ptr != Other.Ptr;}
	bool operator==(const WeakEntPtr& Other) const
	{return Ptr == Other.Ptr;}
	bool operator!=(const WeakEntPtr& Other) const
	{return Ptr != Other.Ptr;}
	bool operator==(const gentity_t *entity) const
	{return Ptr == entity;}
	bool operator!=(const gentity_t *entity) const
	{return Ptr != entity;}

private:
	const gentity_t	*Ptr;

	void CachedAssertDep() const;

	friend struct ScopedEntityContext;
	friend struct EntPtr;
};

// ===========================================================================

namespace DepGraph
{
	void AddDep(const gentity_t *Depends, const gentity_t *On);
	void RemoveDep(const gentity_t *Depends, const gentity_t *On);
	void RemoveVertex(const gentity_t *Vertex);
	bool OnSameIsland(const gentity_t *Depends, const gentity_t *On);
	void RebuildIslands();
}

typedef std::vector<gentity_t *>	EntityIsland;

// ===========================================================================

struct EntityContext
{
	static gentity_t *& GetEntity()
	{return Context.local();}

private:
	static tbb::enumerable_thread_specific<gentity_t *> Context;
};

struct ScopedEntityContext
{
	ScopedEntityContext(const EntPtr& Current)
		: PrevPtr(EntityContext::GetEntity())
	{EntityContext::GetEntity() = Current.Ptr;}

	~ScopedEntityContext()
	{EntityContext::GetEntity() = PrevPtr;}

private:
	gentity_t	*PrevPtr;
};

// ===========================================================================

class ProcessIslandsTask : public tbb::task
{
public:
	typedef std::function<void(EntityIsland *)> PerIslandFunc;

	ProcessIslandsTask(PerIslandFunc PerIsland)
		: Func(PerIsland)
	{}

	tbb::task *execute();

	static void Run(PerIslandFunc PerIsland);

	PerIslandFunc Func;
};
