/**
 * For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file InventoryFolder.h
 *  @brief A class representing folder in the inventory item tre model.
 */

#ifndef incl_InventoryModule_InventoryFolder_h
#define incl_InventoryModule_InventoryFolder_h

#include "AbstractInventoryItem.h"
#include "InventoryModuleApi.h"
#include "RexTypes.h"

namespace Inventory
{
    class InventoryAsset;

    class INVENTORY_MODULE_API InventoryFolder : public AbstractInventoryItem
    {
        Q_OBJECT
        Q_PROPERTY(bool dirty_ READ IsDirty WRITE SetDirty)

    public:
        /// Constructor.
        /// @param data_model Data model.
        /// @param id ID.
        /// @param name Name.
        /// @param parent Parent folder.
        InventoryFolder(const QString &id, const QString &name = "New Folder", InventoryFolder *parent = 0,
            const bool editable = true);

        /// Destructor.
        virtual ~InventoryFolder();

        /// AbstractInventoryItem override
        QString GetName() const { return name_; }

        /// AbstractInventoryItem override
        void SetName(const QString &name) { name_ = name; }

        /// AbstractInventoryItem override
        QString GetID() const { return id_; }

        /// AbstractInventoryItem override
        void SetID(const QString &id) { id_ = id; }

        /// AbstractInventoryItem override
        AbstractInventoryItem *GetParent() const { return parent_; }

        /// AbstractInventoryItem override
        void SetParent(AbstractInventoryItem *parent) { parent_ = parent; }

        /// AbstractInventoryItem override
        bool IsEditable() const { return editable_; }

        /// AbstractInventoryItem override
        void SetEditable(const bool editable) { editable_ = editable; }

        /// AbstractInventoryItem override
        bool IsLibraryItem() const { return libraryItem_; }

        /// AbstractInventoryItem override
        void SetIsLibraryItem(const bool value) { libraryItem_ = value; }

        /// AbstractInventoryItem override
        bool IsDescendentOf(AbstractInventoryItem *searchFolder) const;

        /// AbstractInventoryItem override
        InventoryItemType GetItemType() const { return itemType_; }

        // InventoryFolder API

        /// @return Is this folder dirty.
        bool IsDirty() const { return dirty_; }

        /// Sets the folder dirty flag.
        void SetDirty(const bool &dirty) { dirty_ = dirty; }

        /// Adds new child.
        /// @param child Child to be added.
        /// @return Pointer to the new child.
        AbstractInventoryItem *AddChild(AbstractInventoryItem *child);

        /// Deletes child.
        ///\todo Refactor so that we don't use count.
        /// @param position 
        /// @param count 
        /// @return True if removing is succesfull, false otherwise.
        /// @note It's not recommended to use this directly. This function is used by InventoryItemModel::removeRows().
        bool RemoveChildren(int position, int count);

        /// Deletes child.
        /// @param child Child to be deleted.
//        void DeleteChild(AbstractInventoryItem *child);

        /// Deletes child.
        /// @param id of the child to be deleted.
//        void DeleteChild(const QString &id);

        /// @return First folder by the requested name or null if the folder isn't found.
        /// @param name Search name.
        /// @return Pointer to requested folder, or null if not found.
        InventoryFolder *GetFirstChildFolderByName(const QString &name) const;

        /// Returns pointer to requested folder.
        /// @param searchId Search ID.
        /// @return Pointer to the requested folder, or null if not found.
        InventoryFolder *GetChildFolderById(const QString &searchId) const;

        /// Returns pointer to requested asset.
        /// @param searchId Search ID.
        /// @return Pointer to the requested asset, or null if not found.
        /// @note Non-recursive.
        InventoryAsset *GetChildAssetById(const QString &searchId) const;

        /// Returns pointer to requested child item.
        /// @param searchId Search ID.
        /// @return Pointer to the requested item, or null if not found.
        AbstractInventoryItem *GetChildById(const QString &searchId) const;

        /// Returns the first asset with the requested asset ID.
        /// @param id Asset ID.
        /// @return First asset with the wanted asset ID, or null if not found.
        /// @note Recursive.
        InventoryAsset *GetFirstAssetByAssetId(const QString &id) const;

        /// Returns list of children with the spesific asset type. Searches all subfolders.
        /// @param type Asset type.
        QList<const InventoryAsset *> GetChildAssetsByAssetType(const asset_type_t type) const;

        /// Returns list of children with the spesific inventory type. Searches all subfolders.
        /// @param type Inventory type.
        QList<const InventoryAsset *> GetChildAssetsByInventoryType(const inventory_type_t type) const;

        /// @param row Row number of wanted child.
        /// @return Child item.
        AbstractInventoryItem *Child(int row) const;

        /// @return Number of children.
        int ChildCount() const { return children_.count(); }

        /// @return Does this folder have children.
        bool HasChildren() const { return ChildCount() > 0; }

        /// @return Row number of the folder.
        int Row() const;

        /// @return folders child list 
        /// @todo Should not be public/exist but WebDAV seems to need this at the moment.
        QList<AbstractInventoryItem *> &GetChildren() { return children_; }

#ifdef _DEBUG
        /// Prints the inventory tree structure to std::cout.
        void DebugDumpInventoryFolderStructure(int indentationLevel);
#endif

    private:
        Q_DISABLE_COPY(InventoryFolder);

        /// Type of item (folder or asset)
        InventoryItemType itemType_;

        /// List of children.
        QList<AbstractInventoryItem *> children_;

        /// Dirty flag.
        bool dirty_;

        /// Library asset flag.
        bool libraryItem_;
    };
}

#endif
