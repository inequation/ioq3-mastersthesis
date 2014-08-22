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
	{return GetROPointer() == Other.GetROPointer();}
	bool operator!=(const EntPtr& Other) const
	{return GetROPointer() != Other.GetROPointer();}
	bool operator==(const gentity_t *entity) const;
	bool operator!=(const gentity_t *entity) const;

	// Explicitly fetches pointers of given type. Use with care!
	gentity_t *GetRWPointer() const;
	const gentity_t *GetROPointer() const;

private:
	gentity_t	*Ptr;

	friend class ScopedEntityContext;
};

// ===========================================================================

namespace Mutation
{
	void SetBytes(EntPtr Entity, size_t Offset, size_t Length, void *Data);
	void SetBytes(struct gclient_s *Client, size_t Offset, size_t Length, void *Data);
	void ClearBytes(EntPtr Entity, size_t Offset, size_t Length);
	void ClearBytes(struct gclient_s *Client, size_t Offset, size_t Length);
	void CommitMutations();

	template <class T>
	void Set(EntPtr Entity, size_t Offset, T Value)
	{SetBytes(Entity, Offset, sizeof(T), &Value);}
	template <class T>
	void Set(struct gclient_s *Client, size_t Offset, T Value)
	{SetBytes(Client, Offset, sizeof(T), &Value);}
}

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
