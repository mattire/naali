/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_Touchable.cpp
 *  @brief  EC_Touchable enables visual effect for scene entity in cases where 
 *          the entity can be considered to have some kind of functionality
 *          when it is clicked i.e. touched. The effect is not visible by default.
 *          You must call Show() function separately. The effect is visible only
 *          for certain time.
 *  @note   The entity must have EC_OgrePlaceable and EC_OgreMesh (if mesh) or
 *          EC_OgreCustomObject (if prim) components available in advance.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "EC_Touchable.h"

#include "ModuleInterface.h"
#include "Entity.h"
#include "Renderer.h"
//#include "OgreMaterialUtils.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgreMesh.h"
#include "EC_OgreCustomObject.h"
#include "LoggingFunctions.h"

DEFINE_POCO_LOGGING_FUNCTIONS("EC_Touchable");

#include <QApplication>
#include <QTimer>

#include "MemoryLeakCheck.h"

EC_Touchable::~EC_Touchable()
{
    // OgreRendering module might be already deleted. If so, the cloned entity is also already deleted.
    // In this case, just set pointer to 0.
    if (!renderer_.expired())
    {
        Ogre::SceneManager *sceneMgr = renderer_.lock()->GetSceneManager();
        if (entityClone_)
            sceneMgr->destroyEntity(entityClone_);
    }
    else
    {
        entityClone_ = 0;
        sceneNode_ = 0;
    }
}

void EC_Touchable::OnHover()
{
    if (!IsVisible())
        Show();
    if (!hovering_)
        OnHoverIn();
    emit MouseHover();
}


void EC_Touchable::OnHoverIn()
{
    if (hovering_)
        return;
    hovering_ = true;
    SetCursorVisible(true);
    emit MouseHoverIn();
}

void EC_Touchable::OnHoverOut()
{
    if (IsVisible())
        Hide();
    hovering_ = false;
    SetCursorVisible(false);
    emit MouseHoverOut();
}

void EC_Touchable::OnClick()
{
    emit Clicked();
}

void EC_Touchable::SetCursorVisible(bool visible)
{
    if (hover_cursor_.shape() != Qt::ArrowCursor)
    {
        QCursor *current_cursor = QApplication::overrideCursor();
        if (visible)
        {
            if (current_cursor)
            {
                if (current_cursor->shape() != hover_cursor_.shape())
                    QApplication::setOverrideCursor(hover_cursor_);
            }
            else
                QApplication::setOverrideCursor(hover_cursor_);
        }
        else
        {
            while (current_cursor)
            {
                QApplication::restoreOverrideCursor();
                current_cursor = QApplication::overrideCursor();
            }
        }
    }
}

void EC_Touchable::Show()
{
    if (show_material_)
    {
        if (!entityClone_)
            Create();

        if (!entityClone_)
        {
            LogError("EC_Touchable not initialized properly.");
            return;
        }

        if (entityClone_ && sceneNode_)
            sceneNode_->getAttachedObject(cloneName_)->setVisible(true);
    }
}

void EC_Touchable::Hide()
{
    if (entityClone_ && sceneNode_)
        sceneNode_->getAttachedObject(cloneName_)->setVisible(false);
}

bool EC_Touchable::IsVisible() const
{
    if (entityClone_ && sceneNode_)
        return sceneNode_->getAttachedObject(cloneName_)->isVisible();
    return false;
}

void EC_Touchable::UpdateMaterial()
{
    if (!entityClone_ ||!sceneNode_)
        return;
    try
    {
        entityClone_->setMaterialName(materialName.Get().toStdString());
    }
    catch (Ogre::Exception &e)
    {
        LogError("Could not set material \"" + materialName.Get().toStdString() + "\": " + std::string(e.what()));
        return;
    }
}

EC_Touchable::EC_Touchable(Foundation::ModuleInterface *module) :
    Foundation::ComponentInterface(module->GetFramework()),
    entityClone_(0),
    sceneNode_(0),
    materialName(this, "material name", "Touchable"),
    show_material_(true),
    hovering_(false),
    hover_cursor_(QCursor(Qt::ArrowCursor))
{
    renderer_ = module->GetFramework()->GetServiceManager()->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer);
    connect(this, SIGNAL(OnChanged()), SLOT(UpdateMaterial()));
}

void EC_Touchable::SetHighlightOnHover(bool enabled)
{
    show_material_ = enabled;
}

void EC_Touchable::SetHoverCursor(Qt::CursorShape shape)
{
    hover_cursor_ = QCursor(shape);
}

void EC_Touchable::Create()
{
    if (renderer_.expired())
        return;

    Ogre::SceneManager *scene = renderer_.lock()->GetSceneManager();
    assert(scene);
    if (!scene)
        return;

    Scene::Entity *entity = GetParentEntity();
    assert(entity);
    if (!entity)
        return;

    OgreRenderer::EC_OgrePlaceable *placeable = entity->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
    assert(placeable);
    if (!placeable)
        return;

    // Check out if this entity has EC_OgreMesh or EC_OgreCustomObject.
    Ogre::Entity *originalEntity  = 0;
    if (entity->GetComponent(OgreRenderer::EC_OgreMesh::TypeNameStatic()))
    {
        OgreRenderer::EC_OgreMesh *ec_mesh= entity->GetComponent<OgreRenderer::EC_OgreMesh>().get();
        assert(ec_mesh);

        originalEntity = ec_mesh->GetEntity();
        sceneNode_ = ec_mesh->GetAdjustmentSceneNode();
    }
    else if(entity->GetComponent(OgreRenderer::EC_OgreCustomObject::TypeNameStatic()))
    {
        OgreRenderer::EC_OgreCustomObject *ec_custom = entity->GetComponent<OgreRenderer::EC_OgreCustomObject>().get();
        assert(ec_custom);
        if (!ec_custom->IsCommitted())
        {
            LogError("Mesh entity have not been created for the target primitive. Cannot create EC_Touchable.");
            return;
        }

        originalEntity = ec_custom->GetEntity();
        sceneNode_ = placeable->GetSceneNode();
    }
    else
    {
        LogError("This entity doesn't have either EC_OgreMesh or EC_OgreCustomObject present. Cannot create EC_Touchable.");
        return;
    }

    assert(originalEntity && sceneNode_);
    if (!originalEntity || !sceneNode_)
        return;

    // Clone the Ogre entity.
    cloneName_ = std::string("entity") + renderer_.lock()->GetUniqueObjectName();
    entityClone_ = originalEntity->clone(cloneName_);
    assert(entityClone_);

    // Disable casting of shadows for the clone.
    entityClone_->setCastShadows(false);

    ///\todo If original entity has skeleton, (try to) link it to the clone.
/*
    if (originalEntity->hasSkeleton())
    {
        Ogre::SkeletonInstance *skel = originalEntity->getSkeleton();
        // If sharing a skeleton, force the attachment mesh to use the same skeleton
        // This is theoretically quite a scary operation, for there is possibility for things to go wrong
        Ogre::SkeletonPtr entity_skel = originalEntity->getMesh()->getSkeleton();
        if (entity_skel.isNull())
        {
            LogError("Cannot share skeleton for attachment, not found");
        }
        else
        {
            try
            {
                entityClone_->getMesh()->_notifySkeleton(entity_skel);
            }
            catch (Ogre::Exception &e)
            {
                LogError("Could not set shared skeleton for attachment: " + std::string(e.what()));
            }
        }
    }
*/

    try
    {
        //OgreRenderer::CloneMaterial("Touchable", materialName.Get());
        entityClone_->setMaterialName(materialName.Get().toStdString());
    }
    catch (Ogre::Exception &e)
    {
        LogError("Could not set material \"" + materialName.Get().toStdString() + "\": " + std::string(e.what()));
        return;
    }

    sceneNode_->attachObject(entityClone_);
    Hide();
}

