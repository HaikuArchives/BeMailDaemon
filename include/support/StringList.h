#include <Flattenable.h>

class BList;

class StringList : public BFlattenable {
	public:
		StringList(int32 itemsPerBlock = 20);
		StringList(const StringList&);
		
		~StringList(void);
		
		StringList	&operator=(const StringList &from);
		
/* Flattenable stuff */
		virtual	bool		IsFixedSize() const; //--false for obvious reasons
		virtual	type_code	TypeCode() const;
		virtual	ssize_t		FlattenedSize() const;
		virtual	status_t	Flatten(void *buffer, ssize_t size) const;
		virtual	bool		AllowsTypeCode(type_code code) const;
		virtual	status_t	Unflatten(type_code c, const void *buf, ssize_t size);

/* Adding and removing items. */		
		bool	AddItem(const char *item);
		bool	AddItem(const char *item, int32 atIndex);
		bool	AddList(StringList *newItems);
		bool	AddList(StringList *newItems, int32 atIndex);
		bool	RemoveItem(const char *item);
		bool	RemoveItem(int32 index);
		bool	RemoveItems(int32 index, int32 count);
		bool	ReplaceItem(int32 index, const char *newItem);
		void	MakeEmpty();
		
/* Reordering items. */
		void	SortItems(int (*cmp)(const char *, const char *));
		bool	SwapItems(int32 indexA, int32 indexB);
		bool	MoveItem(int32 fromIndex, int32 toIndex);

/* Retrieving items. */
		const char *ItemAt(int32) const;
		const char *ItemAtFast(int32) const;
		const char *FirstItem() const;
		const char *LastItem() const;
		const char *Items() const;

/* Querying the list. */
		bool	HasItem(const char *item) const;
		int32	IndexOf(const char *item) const;
		int32	CountItems() const;
		bool	IsEmpty() const;

/* Determining differences between lists */
		void NotHere(StringList &other_list, StringList *results);
		void NotThere(StringList &other_list, StringList *results);
		
/* Useful list logic operators */
		StringList	&operator += (const char *item);
		StringList	&operator += (StringList &list);
		
		StringList	&operator -= (const char *item);
		StringList	&operator -= (StringList &list);
		
		StringList	operator | (StringList &list);
		StringList	&operator |= (StringList &list);
		
		StringList	operator ^ (StringList &list);
		StringList	&operator ^= (StringList &list);
		
		bool		operator == (StringList &list);
		const char *operator [] (int32 index);
		
	private:
		BList *strings;
};