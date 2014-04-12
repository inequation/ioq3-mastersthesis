#include "g_local.h"
#include "EntPtr.h"

EntPtr::EntPtr()
	: Ptr(nullptr)
{

}

EntPtr::EntPtr(gentity_t *entity)
	: Ptr(entity)
{

}

EntPtr::EntPtr(const EntPtr& Other)
	: Ptr(Other.Ptr)
{

}

EntPtr::~EntPtr()
{

}

EntPtr& EntPtr::operator=(gentity_t *entity)
{
	Ptr = entity;
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
