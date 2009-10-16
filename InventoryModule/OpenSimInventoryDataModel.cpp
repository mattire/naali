// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file OpenSimInventoryDataModel.cpp
 *  @brief Data model representing the hierarchy of an OpenSim inventory.
 */

#include "StableHeaders.h"
#include "OpenSimInventoryDataModel.h"
#include "AbstractInventoryItem.h"
#include "InventoryModule.h"
#include "RexLogicModule.h"
#include "InventorySkeleton.h"
#include "RexUUID.h"

namespace Inventory
{

OpenSimInventoryDataModel::OpenSimInventoryDataModel(RexLogic::RexLogicModule *rex_logic_module) :
    rexLogicModule_(rex_logic_module),
    rootFolder_(0)
{
    SetupModelData(rexLogicModule_->GetInventory().get());
}

// virtual
OpenSimInventoryDataModel::~OpenSimInventoryDataModel()
{
    delete rootFolder_;
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetFirstChildFolderByName(const QString &searchName) const
{
    return rootFolder_->GetFirstChildFolderByName(searchName);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetChildFolderByID(const QString &searchId) const
{
    return rootFolder_->GetChildFolderByID(searchId);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetMyInventoryFolder() const
{
    return rootFolder_->GetFirstChildFolderByName("My Inventory");
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetTrashFolder() const
{
    return rootFolder_->GetFirstChildFolderByName("Trash");
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetOrCreateNewFolder(const QString &id, AbstractInventoryItem &parentFolder,
    const QString &name, const bool &notify_server)
{
    // Return an existing folder if one with the given id is present.
    InventoryFolder *existing = dynamic_cast<InventoryFolder *>(GetChildFolderByID(id));
    if (existing)
        return existing;

    // Create a new folder.
    InventoryFolder *parent = static_cast<InventoryFolder *>(&parentFolder);
    InventoryFolder *newFolder = new InventoryFolder(id, name, true, parent);

    // Inform the server.
    // We don't want to notify server if we're creating folders "ordered" by server via InventoryDescecendents packet.
    if (notify_server)
        rexLogicModule_->GetServerConnection()->SendCreateInventoryFolderPacket(
            RexUUID(parent->GetID().toStdString()), RexUUID(newFolder->GetID().toStdString()),
            255, newFolder->GetName().toStdString().c_str());

    return parent->AddChild(newFolder);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetOrCreateNewAsset(
    const QString &inventory_id,
    const QString &asset_id,
    AbstractInventoryItem &parentFolder,
    const QString &name)
{
    // Return an existing asset if one with the given id is present.
    InventoryFolder *parent = static_cast<InventoryFolder *>(&parentFolder);
    InventoryAsset *existing = dynamic_cast<InventoryAsset *>(parent->GetChildAssetByID(inventory_id));
    if (existing)
        return existing;

    // Create a new asset.
    InventoryAsset *newAsset = new InventoryAsset(inventory_id, asset_id, name, parent);

    return parent->AddChild(newAsset);
}

void OpenSimInventoryDataModel::FetchInventoryDescendents(AbstractInventoryItem *folder)
{
    InventoryFolder *requestedFolder = static_cast<InventoryFolder *>(folder);

    rexLogicModule_->GetServerConnection()->SendFetchInventoryDescendentsPacket(RexUUID(folder->GetID().toStdString()),
        RexUUID(folder->GetParent()->GetID().toStdString()), 0 , true, true);

/*
    rexLogicModule_->GetServerConnection()->SendFetchInventoryDescendentsPacket(RexUUID(folder->GetID().toStdString()),
        RexUUID(folder->GetParent()->GetID().toStdString()), 0 , false, true);
*/
}

void OpenSimInventoryDataModel::NotifyServerAboutItemRemoval(AbstractInventoryItem *item)
{
    // When deleting items, we move them first to the Trash folder.
    // If the folder is already in the trash folder, delete it for good.
    ///\todo Move the "deleted" folder to the Trash folder and update the view.
    InventoryFolder *trashFolder = static_cast<InventoryFolder *>(GetTrashFolder());

    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
    {
        if (!trashFolder)
        {
            InventoryModule::LogError("Can't find Trash folder. Moving folder to Trash not possible. Deleting folder.");
            rexLogicModule_->GetServerConnection()->SendRemoveInventoryFolderPacket(QSTR_TO_UUID(item->GetID()));
            return;
        }

        if (item->GetParent() == trashFolder)
            rexLogicModule_->GetServerConnection()->SendRemoveInventoryFolderPacket(QSTR_TO_UUID(item->GetID()));
        else
            rexLogicModule_->GetServerConnection()->SendMoveInventoryFolderPacket(QSTR_TO_UUID(item->GetID()),
                QSTR_TO_UUID(trashFolder->GetID()));
        return;
    }

    if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
    {
        if (!trashFolder)
        {
            InventoryModule::LogError("Can't find Trash folder. Moving folder to Trash not possible. Deleting asset.");
            rexLogicModule_->GetServerConnection()->SendRemoveInventoryItemPacket(QSTR_TO_UUID(item->GetID()));
            return;
        }

        if (item->GetParent() == trashFolder)
            rexLogicModule_->GetServerConnection()->SendRemoveInventoryItemPacket(QSTR_TO_UUID(item->GetID()));
        else
            rexLogicModule_->GetServerConnection()->SendMoveInventoryItemPacket(QSTR_TO_UUID(item->GetID()),
                QSTR_TO_UUID(trashFolder->GetID()), item->GetName().toStdString());
    }
}

void OpenSimInventoryDataModel::DebugDumpInventoryFolderStructure()
{
    rootFolder_->DebugDumpInventoryFolderStructure(0);
}

void OpenSimInventoryDataModel::CreateNewFolderFromFolderSkeleton(
    InventoryFolder *parent_folder,
    OpenSimProtocol::InventoryFolderSkeleton *folder_skeleton)
{
    using namespace OpenSimProtocol;
    
    InventoryFolder *newFolder= new InventoryFolder(STD_TO_QSTR(folder_skeleton->id.ToString()),
        STD_TO_QSTR(folder_skeleton->name), folder_skeleton->editable, parent_folder);
    //if (!folder_skeleton->HasChildren())
    newFolder->SetDirty(true);

    if (!rootFolder_ && !parent_folder)
        rootFolder_ = newFolder;

    if (parent_folder)
        parent_folder->AddChild(newFolder);

    InventoryFolderSkeleton::FolderIter iter = folder_skeleton->children.begin();
    while(iter != folder_skeleton->children.end())
    {
        parent_folder = newFolder;
        CreateNewFolderFromFolderSkeleton(parent_folder, &*iter);
        ++iter;
    }
}

void OpenSimInventoryDataModel::SetupModelData(OpenSimProtocol::InventorySkeleton *inventory_skeleton)
{
    OpenSimProtocol::InventoryFolderSkeleton *root_skel = inventory_skeleton->GetRoot();
    if (!root_skel)
    {
        InventoryModule::LogError("Couldn't find inventory root folder skeleton. Can't create OpenSim inventory data model.");
        return;
    }

    CreateNewFolderFromFolderSkeleton(0, root_skel);
}

} // namespace Inventory
