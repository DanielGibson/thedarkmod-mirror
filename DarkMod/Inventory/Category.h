/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/
#ifndef __DARKMOD_INVENTORYCATEGORY_H__
#define __DARKMOD_INVENTORYCATEGORY_H__

#include "Item.h"

/**
 * InventoryCategory is just a container for items that are currently held by an entity.
 * We put the constructor and PutItem into the protected scope, so we can ensure
 * that groups are only created by using the inventory class.
 */
class CInventoryCategory
{
	friend class CInventory;

private:
	CInventoryCategory(CInventory* inventory, const idStr& name = "");

public:
	~CInventoryCategory();

	inline const idStr&	GetName() { return m_Name; }
	inline void			SetInventory(CInventory *Inventory) { m_Inventory = Inventory; };

	inline idEntity	*		GetOwner() { return m_Owner.GetEntity(); };

	// Look up an InventoryItem by its ItemId (NOT equivalent to GetItem(const idStr& Name) btw).
	CInventoryItemPtr		GetItemById(const idStr& id);

	// Look up an InventoryItem by <name> or <index>
	CInventoryItemPtr		GetItem(const idStr& itemName);
	CInventoryItemPtr		GetItem(int index);

	// Returns the index of the given item or the item with the given name. Returns -1 if not found.
	int						GetItemIndex(const idStr& itemName);
	int						GetItemIndex(const CInventoryItemPtr& item);

	// Returns the sum of all gold, loot and jewelry of this category plus the gold, loot, jewelry sums on their own.
	int						GetLoot(int& gold, int& jewelry, int& goods);

	/**
	 * greebo: Adds the given item to this category
	 */
	void					PutItem(CInventoryItemPtr Item);
	/**
	 * greebo: Removes the specified <item> from this category.
	 */
	void					RemoveItem(const CInventoryItemPtr& item);

	/** greebo: Returns true if the category contains no items.
	 */
	bool					IsEmpty() const;

	/**
	 * Returns the number of items in this category.
	 */
	int						GetNumItems() const;

	void					Save(idSaveGame *savefile) const;
	void					Restore(idRestoreGame *savefile);

private:
	void					SetOwner(idEntity *Owner);

private:
	CInventory*				m_Inventory;			// The inventory this group belongs to.
	idEntityPtr<idEntity>	m_Owner;

	// The name of this group.
	idStr					m_Name;

	// A list of contained items (are deleted on destruction of this object).
	idList<CInventoryItemPtr>	m_Item;
};
typedef boost::shared_ptr<CInventoryCategory> CInventoryCategoryPtr;

#endif /* __DARKMOD_INVENTORYCATEGORY_H__ */
