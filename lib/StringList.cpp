#include <List.h>

#include <string.h>
#include <malloc.h>

class _EXPORT StringList;

#include "StringList.h"

StringList::StringList(int32 itemsPerBlock) : 
	strings(new BList(itemsPerBlock)) {
		//----do nothing, and do it well------
	}
	
StringList::StringList(const StringList& from) :
	strings(new BList) {
		*this = from;
	}

StringList	&StringList::operator=(const StringList &from) {
	MakeEmpty();
	for (int32 i = 0; i < from.CountItems(); i++)
		strings->AddItem(strdup(from.ItemAt(i)));
		
	return *this;
}

bool		StringList::IsFixedSize() const {
	return false;
}

type_code	StringList::TypeCode() const {
	return 'STRL';
}

ssize_t		StringList::FlattenedSize() const {
	ssize_t size = 0;
	for (int32 i = 0; i < CountItems(); i++)
		size += (strlen(ItemAt(i)) + 1);
		
	return size;
}

status_t	StringList::Flatten(void *buffer, ssize_t size) const {
	if (size < FlattenedSize())
		return B_NO_MEMORY;
		
	for (int32 i = 0; i < CountItems(); i++) {
		memcpy(buffer, ItemAt(i), strlen(ItemAt(i)) + 1);
		buffer = (void *)((const char *)(buffer) + (strlen(ItemAt(i)) + 1));
	}
	
	return B_OK;
}

bool		StringList::AllowsTypeCode(type_code code) const {
	return (code == 'STRL');
}

status_t	StringList::Unflatten(type_code c, const void *buf, ssize_t size) {
	if (c != 'STRL')
		return B_ERROR;
		
	const char *string = (const char *)(buf);
	
	for (off_t offset = 0; offset < size; offset ++) {
		if (((int8 *)(buf))[offset] == 0) {
			AddItem(string);
			string = (const char *)buf + offset + 1;
		}
	}
	
	return B_OK;
}	

bool	StringList::AddItem(const char *item) {
	return strings->AddItem(strdup(item));
}

bool	StringList::AddItem(const char *item, int32 atIndex) {
	return strings->AddItem(strdup(item),atIndex);
}

bool	StringList::AddList(StringList *newItems) {
	for (int32 i = 0; i < newItems->CountItems(); i++)
		strings->AddItem(strdup(newItems->ItemAt(i)));
		
	return true;
}

bool	StringList::AddList(StringList *newItems, int32 atIndex) {
	for (int32 i = newItems->CountItems() - 1; i >= 0; i--)
		strings->AddItem(strdup(newItems->ItemAt(i)),atIndex);

	return true;
}

bool	StringList::RemoveItem(const char *item) {
	int32 index = -1;
	for (int32 i = 0; i < CountItems(); i++) {
		if (strcmp(ItemAt(i),item) == 0) {
			index = i;
			break;
		}
	}
	
	return RemoveItem(index);
}

bool StringList::RemoveItem(int32 index) {
	void *item = strings->RemoveItem(index);
	if (item == NULL)
		return false;
		
	free(item);
	return true;
}
	
bool	StringList::RemoveItems(int32 index, int32 count) {
	for (int32 i = 0; i < count; i++) {
		if (!RemoveItem(index))
			return false;
	}
	
	return true;
}

bool	StringList::ReplaceItem(int32 index, const char *newItem) {
	if (!RemoveItem(index))
		return false;
		
	AddItem(newItem,index);
	return true;
}

void	StringList::MakeEmpty() {
	for (int32 i = 0; i < CountItems(); i++)
		free(strings->ItemAt(i));
		
	strings->MakeEmpty();
}

void	StringList::SortItems(int (*cmp)(const char *, const char *)) {
	strings->SortItems((int (*)(const void *,const void *))(cmp));
}

bool	StringList::SwapItems(int32 indexA, int32 indexB) {
	return strings->SwapItems(indexA,indexB);
}

bool	StringList::MoveItem(int32 fromIndex, int32 toIndex) {
	return strings->MoveItem(fromIndex,toIndex);
}

const char *StringList::ItemAt(int32 index) const {
	return (const char *)(strings->ItemAt(index));
}

const char *StringList::ItemAtFast(int32 index) const {
	return (const char *)(strings->ItemAtFast(index));
}

const char *StringList::FirstItem() const {
	return (const char *)(strings->FirstItem());
}

const char *StringList::LastItem() const {
	return (const char *)(strings->LastItem());
}

const char *StringList::Items() const {
	return (const char *)(strings->Items());
}

bool	StringList::HasItem(const char *item) const {
	return (IndexOf(item) > -1);
}

int32	StringList::IndexOf(const char *item) const {
	int32 index = -1;
	for (int32 i = 0; i < CountItems(); i++) {
		if (strcmp(ItemAt(i),item) == 0) {
			index = i;
			break;
		}
	}
	
	return index;
}

int32	StringList::CountItems() const {
	return strings->CountItems();
}

bool	StringList::IsEmpty() const {
	return strings->IsEmpty();
}

void StringList::NotHere(StringList &other_list, StringList *results) {
	for (int32 i = 0; i < other_list.CountItems(); i++) {
		if (!HasItem(other_list[i]))
			results->AddItem(other_list[i]);
	}
}
	
void StringList::NotThere(StringList &other_list, StringList *results) {
	other_list.NotHere(*this,results);
}

StringList	&StringList::operator += (const char *item) {
	AddItem(item);
	return *this;
}

StringList	&StringList::operator += (StringList &list) {
	AddList(&list);
	return *this;
}

StringList	&StringList::operator -= (const char *item) {
	RemoveItem(item);
	return *this;
}

StringList	&StringList::operator -= (StringList &list) {
	for (int32 i = 0; i < list.CountItems(); i++)
		RemoveItem(list[i]);
		
	return *this;
}

StringList	StringList::operator | (StringList &list2) {
	StringList list(*this);
	for (int32 i = 0; i < list2.CountItems(); i++) {
		if (!list.HasItem(list2.ItemAt(i)))
			list += list2.ItemAt(i);
	}
	
	return list;
}

StringList	&StringList::operator |= (StringList &list2) {
	for (int32 i = 0; i < list2.CountItems(); i++) {
		if (!HasItem(list2.ItemAt(i)))
			AddItem(list2.ItemAt(i));
	}
	
	return *this;
}

StringList StringList::operator ^ (StringList &list2) {
	StringList list;
	for (int32 i = 0; i < CountItems(); i++) {
		if (!list2.HasItem(ItemAt(i)))
			list += ItemAt(i);
	}
	for (int32 i = 0; i < list2.CountItems(); i++) {
		if (!HasItem(list2.ItemAt(i)))
			list += list2.ItemAt(i);
	}
	return list;
}

StringList &StringList::operator ^= (StringList &list) {
	return (*this = *this ^ list);
}

bool StringList::operator == (StringList &list) {
	if (list.CountItems() != CountItems())
		return false;
		
	for (int32 i = 0; i < CountItems(); i++) {
		if (strcmp(list.ItemAt(i),ItemAt(i)) != 0)
			return false;
	}
	
	return true;
}

const char *StringList::operator [] (int32 index) {
	return ItemAt(index);
}

StringList::~StringList() {
	for (int32 i = 0; i < CountItems(); i++)
		free(strings->ItemAt(i));
}