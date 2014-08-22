#pragma once

#include <tbb/enumerable_thread_specific.h>

class EntPtr
{
public:
	EntPtr();
	EntPtr(gentity_t *entity);
	EntPtr(const EntPtr& Other);
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

	friend class ScopedEntityContext;
};

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
