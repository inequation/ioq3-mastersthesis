#include "g_local.h"
#include "EntPtr.h"

tbb::enumerable_thread_specific<gentity_t *> EntityContext::Context;

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
