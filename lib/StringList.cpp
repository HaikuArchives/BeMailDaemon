#include <List.h>

#include <string.h>
#include <malloc.h>
#include <assert.h>

class _EXPORT StringList;

#include "StringList.h"

uint8 string_hash(const char *string);

uint8 string_hash(const char *string) {
	uint8 hash = 0;
	for (int i = 0; string[i] != 0; i++)
		hash ^= string[i];
		
	return hash;
}

struct string_bucket {
	const char *string;
	
	struct string_bucket *next;
};

StringList::StringList()
	: BFlattenable(), _items(0), _indexed(new BList)
{
	for (int32 i = 0; i < 256; i++)
		_buckets[i] = NULL;
}
	
StringList::StringList(const StringList& from)
	: BFlattenable(), _items(0), _indexed(new BList)
{
	AddList(&from);
}

StringList	&StringList::operator=(const StringList &from) {
	MakeEmpty();
	
	AddList(&from);
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
	struct string_bucket *bkt;
	for (int32 i = 0; i < 256; i++) {
		bkt = (struct string_bucket *)(_buckets[i]);
		while (bkt != NULL) {
			size += (strlen(bkt->string) + 1);
			bkt = bkt->next;
		}
	}
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

void	StringList::AddItem(const char *item) {
	struct string_bucket *new_bkt;
	
	new_bkt = (struct string_bucket *)_buckets[string_hash(item)];
	if (new_bkt == NULL) {
		new_bkt = new struct string_bucket;
		new_bkt->string = strdup(item);
		_indexed->AddItem((void *)(new_bkt->string));
		new_bkt->next = NULL;
		_buckets[string_hash(item)] = new_bkt;
		
		_items++;
		return;
	}
	
	while (new_bkt->next != NULL) new_bkt = new_bkt->next;
	
	new_bkt->next = new struct string_bucket;
	new_bkt = new_bkt->next;
	new_bkt->string = strdup(item);
	_indexed->AddItem((void *)(new_bkt->string));
	
	new_bkt->next = NULL;
	_items++;
}

void	StringList::AddList(const StringList *newItems) {
	for (int32 i = 0; i < newItems->CountItems(); i++)
		AddItem((const char *)(newItems->ItemAt(i)));
	
	/*struct string_bucket *bkt, *new_bkt;
	for (int32 i = 0; i < 256; i++) {
		bkt = (struct string_bucket *)(newItems->_buckets[i]);
		while (bkt != NULL) {
			new_bkt = (struct string_bucket *)_buckets[i];
			if (new_bkt == NULL) {
				new_bkt = new struct string_bucket;
				new_bkt->string = strdup(bkt->string);
				_indexed->Add
				new_bkt->next = NULL;
				_buckets[i] = new_bkt;
				_items++;
				continue;
			}
			
			while (new_bkt->next != NULL) new_bkt = new_bkt->next;
			
			new_bkt->next = new struct string_bucket;
			new_bkt = new_bkt->next;
			new_bkt->string = strdup(bkt->string);
			new_bkt->next = NULL;
			_items++;
			
			bkt = bkt->next;
		}
	}*/
}

bool	StringList::RemoveItem(const char *item) {
	struct string_bucket *bkt = (struct string_bucket *)_buckets[string_hash(item)];
	
	if (bkt == NULL)
		return false;
	
	if (strcmp(bkt->string,item) == 0) {
		_indexed->RemoveItem(IndexOf(item));
		_buckets[string_hash(item)] = bkt->next;
		free((void *)(bkt->string));
		delete bkt;
		_items--;
		return true;
	}
	
	while (bkt->next != NULL) {
		if (strcmp(bkt->next->string,item) == 0) {
			_indexed->RemoveItem(IndexOf(item));
			
			bkt->next = bkt->next->next;
			free((void *)(bkt->string));
			delete bkt;
			_items--;
			
			return true;
		}
		
		bkt = bkt->next;
	}
	
	return false;
}

void	StringList::MakeEmpty() {
	struct string_bucket *bkt, *next_bkt;
	
	for (int i = 0; i < 256; i++) {
		bkt = (struct string_bucket *)_buckets[i];
		_buckets[i] = NULL;
		
		while (bkt != NULL) {
			free ((void *)(bkt->string));
			next_bkt = bkt->next;
			delete bkt;
			
			bkt = next_bkt;
		}
	}
	
	_indexed->MakeEmpty();
	
	_items = 0;
}

const char *StringList::ItemAt(int32 index) const {
	return (const char *)(_indexed->ItemAt(index));
}

int32 StringList::IndexOf(const char *item) const {
	for (int32 i = 0; i < _indexed->CountItems(); i++) {
		if (strcmp(item,(const char *)_indexed->ItemAt(i)) == 0)
			return i;
	}
	
	return -1;
}

bool	StringList::HasItem(const char *item) const {
	struct string_bucket *bkt = (struct string_bucket *)_buckets[string_hash(item)];
	
	while (bkt != NULL) {
		if (strcmp(bkt->string,item) == 0)
			return true;
			
		bkt = bkt->next;
	}
	
	return false;
}

int32	StringList::CountItems() const {
	return _items;
}

bool	StringList::IsEmpty() const {
	return (_items == 0);
}

void StringList::NotHere(StringList &other_list, StringList *results) {
	#if DEBUG
	 assert(_items == _indexed->CountItems());
	 assert(other_list._items == other_list._indexed->CountItems());
	#endif
	
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
	MakeEmpty();
	
	delete _indexed;
}
