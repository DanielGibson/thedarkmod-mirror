/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 915 $
 * $Date: 2007-04-19 22:10:27 +0200 (Do, 19 Apr 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/
#ifndef __DARKMOD_INVENTORY_H__
#define __DARKMOD_INVENTORY_H__

#include "Cursor.h"

#define TDM_INVENTORY_DEFAULT_GROUP		"DEFAULT"
#define TDM_LOOT_INFO_ITEM				"loot_info"
#define TDM_LOOT_SCRIPTOBJECT			"loot_gui"
#define TDM_DUMMY_ITEM					"dummy"
#define TDM_INVENTORY_DROPSCRIPT		"inventoryDrop"
#define TDM_CATEGORY_AMMO				"ammo"
#define TDM_INVENTORY_AMMO_PREFIX		"inv_ammo_"

 /* DESCRIPTION: This file contains the inventory handling for TDM. The inventory 
 * has nothing in common with the original idInventory and is totally independent
 * from it. It contains the inventory itself, the groups, items and cursors for
 * interaction with the inventory.
 * Each entity has exactly one inventory. An inventory is created when the entitie's
 * inventory is accessed for th first time and also one default group is added 
 * named "DEFAULT".
 *
 * Each item belongs to a group. If no group is specified, then it will be 
 * put in the default group. Each item also knows its owning entity and the
 * entity it references. When an entity is destroyed, it will also destroy
 * its item. Therefore you should never keep a pointer of an item in memory
 * and always fetch it from the inventory when you need it, as you can never 
 * know for sure, that the respective entity hasn't been destroyed yet (or
 * the item itself).
 *
 * Categories and inventories are removed when they are empty. This happens
 * after inventoryDrop() in the idPlayer code.
 *
 * Inventories are accessed via cursors. This way you can have multiple
 * cursors pointing to the same inventory with different tasks. You can
 * also add a ignore filter, so that a given cursor can only cycle through
 * a subset of the existing categories in a given inventory.
 *
 * At the time of writing, there are two implementations of CInventoryItem:
 * The default implementation for loot and ordinary inventory items and
 * items representing weapons (CInventoryWeaponItem). The WeaponItem extend
 * the interface and member variables with weapon-related stuff like ammo-handling.
 * For instance: The idWeapon class requests the active WeaponItem 
 * from the idPlayer class to check whether the active weapon is ready to fire.
 * 
 */

/**
 * An inventory is a container for groups of items. An inventory has one default group
 * which is always created. All other groups are up to the mapper to decide.
 *
 * If an item is put into an inventory, without specifying the group, it will be put
 * in the default group which is always at index 0 and has the name DEFAULT.
 */
class CInventory : public idClass
{
public:
	CLASS_PROTOTYPE(CInventory);

	friend class CInventoryCursor;

	CInventory();
	~CInventory();

	CInventoryCursor		*CreateCursor(void);

	int						GetLoot(int &Gold, int &Jewelry, int &Goods);

	inline idEntity			*GetOwner(void) { return m_Owner.GetEntity(); };
	void					SetOwner(idEntity *Owner);

	/**
	 * CreateCategory creates the named group if it doesn't already exist.
	 */
	CInventoryCategory		*CreateCategory(const idStr& CategoryName, int *Index = NULL);

	/**
	 * greebo: Removes the category from this inventory. Will NOT check whether the
	 *         the category is empty or not.
	 */
	void					removeCategory(CInventoryCategory* category);

	/**
	 * GetCategory returns the pointer to the given group and it's index, 
	 * if the pointer is not NULL.
	 */
	CInventoryCategory	*GetCategory(const idStr& CategoryName, int *Index = NULL);

	/**
	 * Retrieves the category with the given index. Useful for scriptevents to 
	 * reference a certain inventory category.
	 *
	 * @returns: NULL, if the category with the given inedx was not found.
	 */
	CInventoryCategory* GetCategory(int index);

	/**
	 * GetCategoryIndex returns the index to the given group or -1 if not found.
	 */
	int					GetCategoryIndex(const idStr& CategoryName);

	/**
	 * Return the groupindex of the item or -1 if it doesn't exist. Optionally
	 * the itemindex within that group can also be obtained. Both are set to -1 
	 * if the item can not be found. The ItemIndex pointer only when it is not NULL of course.
	 */
	int						GetCategoryItemIndex(CInventoryItem *Item, int *ItemIndex = NULL);
	int						GetCategoryItemIndex(const idStr& ItemName, int *ItemIndex = NULL);

	/**
	 * Remove entity from map will remove the entity from the map. If 
	 * bDelete is set, the entity will be deleted as well. This can only
	 * be done if droppable is set to 0 as well. Otherwise it may be
	 * possible for the player to drop the item, in which case the entity 
	 * must stay around.
	 */
	void					RemoveEntityFromMap(idEntity *ent, bool bDelete = false);

	/**
	 * Puts an item back into the map. This can only be done when the item is
	 * droppable, otherwise it is ignored. If the item was an objective, the 
	 * objective will to be toggled back again. Owner is needed to retrieve the
	 * position where the item will be released.
	 */
	void					PutEntityInMap(idEntity *ent, idEntity *owner, CInventoryItem *item);

	/**
	 * Put an item in the inventory. Use the default group if none is specified.
	 * The name, that is to be displayed on a GUI, must be set on the respective
	 * entity.
	 *
	 * greebo: This routine basically checks all the spawnargs, determines the 
	 *         inventory category, the properties like "droppable" and such.
	 *         If the according spawnarg is set, the entity is removed from the map.
	 *         This can either mean "hide" or "delete", depending on the stackable property.
	 */
	CInventoryItem			*PutItem(idEntity *Item, idEntity *Owner);
	void					PutItem(CInventoryItem *Item, const idStr& category);

	/**
	 * Retrieve an item from an inventory. If no group is specified, all of 
	 * them are searched, otherwise only the given group.
	 */
	CInventoryItem			*GetItem(const idStr& Name, const idStr& Category = "", bool bCreateCategory = false);

	CInventoryItem			*GetItemById(const idStr& Name, const idStr& Category = "", bool bCreateCategory = false);

	void					Save(idSaveGame *savefile) const;
	void					Restore(idRestoreGame *savefile);

protected:

	/**
	 * greebo: This checks the given entity for loot items and adds the loot value 
	 *         to the loot sum.
	 * 
	 * @returns: The standard loot info InventoryItem or NULL if the item is not a valid loot item.
	 */
	CInventoryItem*		ValidateLoot(idEntity *ent);

	/**
	 * greebo: Checks the given entity for ammo definitions. Does not remove the entity.
	 *
	 * @returns: The weaponItem the ammo has been added to or NULL, if <ent> isn't a valid ammo item.
	 */
	CInventoryItem*		ValidateAmmo(idEntity* ent);

protected:
	idEntityPtr<idEntity>				m_Owner;

	idList<CInventoryCursor *>			m_Cursor;

	/**
	 * List of groups in that inventory
	 */
	idList<CInventoryCategory *>		m_Category;

	/**
	 * Here we keep the lootcount for the items, that don't need to actually 
	 * be stored in the inventory, because they can't get displayed anyway.
	 * LootItemCount will only count the items, that are stored here. Items
	 * that are visible, will not be counted here. This is only for stats.
	 */
	int						m_LootItemCount;
	int						m_Gold;
	int						m_Jewelry;
	int						m_Goods;
};

#endif /* __DARKMOD_INVENTORY_H__ */
