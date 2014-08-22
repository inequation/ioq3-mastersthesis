#include "g_local.h"
#include "DoubleBuffering.h"

#include <tbb/concurrent_queue.h>

tbb::enumerable_thread_specific<gentity_t *> EntityContext::Context;

// ===========================================================================

static const gentity_t *GetROPointer(const gentity_t *ent)
{
	if (ent - g_entities_old >= 0 && ent - g_entities_old < MAX_GENTITIES)
		return ent;
	return &g_entities_old[ent - g_entities];
}

static gentity_t *GetRWPointer(const gentity_t *ent)
{
	if (ent - g_entities >= 0 && ent - g_entities < MAX_GENTITIES)
		return (gentity_t *)ent;
	return &g_entities[ent - g_entities_old];
}

static gentity_t *GetProperPointer(const gentity_t *ent)
{
	if (g_entities_old == g_entities)
		return (gentity_t *)ent;	// on first frame never modify the pointer
	if (ent == nullptr)
		return nullptr;
	auto Context = EntityContext::GetEntity();
	return (Context == ent || Context == nullptr)
			?              GetRWPointer(ent)	// writing to ourselves is OK
			: (gentity_t *)GetROPointer(ent);	// writing to others is not OK
}

// ===========================================================================

EntPtr::EntPtr()
	: Ptr(nullptr)
{

}

EntPtr::EntPtr(gentity_t *entity)
	: Ptr(GetProperPointer(entity))
{

}

EntPtr::EntPtr(const EntPtr& Other)
	: Ptr(GetProperPointer(Other.Ptr))
{

}

EntPtr::~EntPtr()
{

}

EntPtr& EntPtr::operator=(gentity_t *entity)
{
	Ptr = GetProperPointer(entity);
	return *this;
}

gentity_t& EntPtr::operator*() const
{
	return *Ptr;
}

gentity_t *EntPtr::operator->() const
{
	return Ptr;
}

EntPtr::operator gentity_t *() const
{
	return Ptr;
}

EntPtr& EntPtr::operator++()
{
	++Ptr;
	return *this;
}

EntPtr EntPtr::operator++(int)
{
	return EntPtr(Ptr++);
}

gentity_t *EntPtr::GetRWPointer() const
{
	return ::GetRWPointer(Ptr);
}

const gentity_t *EntPtr::GetROPointer() const
{
	return ::GetROPointer(Ptr);
}

bool EntPtr::operator==(const gentity_t *entity) const
{
	return GetROPointer() == ::GetROPointer(entity);
}

bool EntPtr::operator!=(const gentity_t *entity) const
{
	return GetROPointer() != ::GetROPointer(entity);
}

// ===========================================================================

extern gclient_t *g_clients;

namespace Mutation
{
	struct Mutation
	{
		enum
		{
			ENTITY,
			CLIENT
		}			Type;
		void		*Object;
		size_t		Offset;
		size_t		Length;
		void		*Data;
	};
	static tbb::concurrent_bounded_queue<Mutation> gMutationQueue;

	void SetBytes(EntPtr Entity, size_t Offset, size_t Length, void *Data)
	{
		auto Context = EntityContext::GetEntity();

		if (Context == nullptr || Context == Entity)
		{
			// fast path – we have RW access, just copy it over
			memcpy((char *)Entity.GetRWPointer() + Offset, Data, Length);
			return;
		}

		// slow path – queue for commit at end of frame
		Mutation m;
		m.Type = Mutation::ENTITY;
		m.Object = Entity;
		m.Offset = Offset;
		m.Length = Length;
		m.Data = malloc(Length);
		memcpy(m.Data, Data, Length);
		gMutationQueue.push(m);
	}

	void SetBytes(struct gclient_s *Client, size_t Offset, size_t Length, void *Data)
	{
		auto Context = EntityContext::GetEntity();

		if (Context == nullptr || Context == &g_entities[Client - g_clients])
		{
			// fast path – we have RW access, just copy it over
			memcpy((char *)Client + Offset, Data, Length);
			return;
		}

		// slow path – queue for commit at end of frame
		Mutation m;
		m.Type = Mutation::CLIENT;
		m.Object = Client;
		m.Offset = Offset;
		m.Length = Length;
		m.Data = malloc(Length);
		memcpy(m.Data, Data, Length);
		gMutationQueue.push(m);
	}

	void ClearBytes(EntPtr Entity, size_t Offset, size_t Length)
	{
		auto Context = EntityContext::GetEntity();

		if (Context == nullptr || Context == Entity)
		{
			// fast path – we have RW access, just copy it over
			memset((char *)Entity.GetRWPointer() + Offset, 0, Length);
			return;
		}

		// slow path – queue for commit at end of frame
		Mutation m;
		m.Type = Mutation::ENTITY;
		m.Object = Entity;
		m.Offset = Offset;
		m.Length = Length;
		m.Data = malloc(Length);
		memset(m.Data, 0, Length);
		gMutationQueue.push(m);
	}

	void ClearBytes(struct gclient_s *Client, size_t Offset, size_t Length)
	{
		auto Context = EntityContext::GetEntity();

		if (Context == nullptr || Context == &g_entities[Client - g_clients])
		{
			// fast path – we have RW access, just copy it over
			memset((char *)Client + Offset, 0, Length);
			return;
		}

		// slow path – queue for commit at end of frame
		Mutation m;
		m.Type = Mutation::CLIENT;
		m.Object = Client;
		m.Offset = Offset;
		m.Length = Length;
		m.Data = malloc(Length);
		memset(m.Data, 0, Length);
		gMutationQueue.push(m);
	}

	void CommitMutations()
	{
		Mutation m;
		while (gMutationQueue.try_pop(m))
		{
			void *Ptr = (char *)(m.Type == Mutation::ENTITY ? ::GetRWPointer((gentity_t *)m.Object) : m.Object) + m.Offset;
			memcpy(Ptr, m.Data, m.Length);
			free(m.Data);
		}
	}
}
