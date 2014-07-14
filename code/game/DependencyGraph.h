#pragma once

#include <tbb/enumerable_thread_specific.h>
#include <vector>

struct WeakEntPtr;

// basic, dependency-enforcing entity smart pointer
struct EntPtr
{
	EntPtr();
	EntPtr(gentity_t *entity);
	EntPtr(const EntPtr& Other);
	EntPtr(const WeakEntPtr& Other);
	~EntPtr();

	EntPtr& operator=(gentity_t *entity);

	gentity_t& operator*() const;
	gentity_t *operator->() const;
	operator gentity_t *() const;
	EntPtr& operator++();
	EntPtr operator++(int);

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

	void AssertDep() const;
	void AutoAddDep() const;
	void AutoRemoveDep() const;

	friend struct WeakEntPtr;
};

// ===========================================================================

// read-only, non-dependency-enforcing (though dependency-checking) smart pointer
struct WeakEntPtr
{
	WeakEntPtr();
	WeakEntPtr(gentity_t *entity);
	WeakEntPtr(const EntPtr& Other);
	WeakEntPtr(const WeakEntPtr& Other);
	~WeakEntPtr();

	WeakEntPtr& operator=(gentity_t *entity);

	const gentity_t& operator*() const;
	const gentity_t *operator->() const;
	operator const gentity_t *() const;
	WeakEntPtr& operator++();
	WeakEntPtr operator++(int);

	bool operator==(const WeakEntPtr& Other) const
	{return Ptr == Other.Ptr;}
	bool operator!=(const WeakEntPtr& Other) const
	{return Ptr != Other.Ptr;}
	bool operator==(const gentity_t *entity) const
	{return Ptr == entity;}
	bool operator!=(const gentity_t *entity) const
	{return Ptr != entity;}

private:
	gentity_t	*Ptr;

	void AssertDep() const;

	friend struct EntPtr;
};

// ===========================================================================

namespace DepGraph
{
	void AddDep(gentity_t *Depends, gentity_t *On);
	void RemoveDep(gentity_t *Depends, gentity_t *On);
	bool IsDependent(gentity_t *Depends, gentity_t *On);
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
	{EntityContext::GetEntity() = Current;}

	~ScopedEntityContext()
	{EntityContext::GetEntity() = PrevPtr;}

private:
	EntPtr	PrevPtr;
};
