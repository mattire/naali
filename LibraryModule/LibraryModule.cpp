/**
*  For conditions of distribution and use, see copyright notice in license.txt
*
*  @file   LibraryModule.cpp
*  @brief  
*/

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "LibraryModule.h"
#include "LibraryWidget.h"

#include "WorldStream.h"
#include "EventManager.h"
#include "UiModule.h"
#include "NetworkMessages/NetInMessage.h"
#include "UiServiceInterface.h"
#include "WorldLogicInterface.h"
#include "ConsoleCommandServiceInterface.h"
#include "AssetServiceInterface.h"
#include "AssetEvents.h"
#include "OgreRenderingModule.h"

#include "Renderer.h"
#include "Ogre.h"
#include "SceneEvents.h"
#include "SceneManager.h"
#include "UiProxyWidget.h"

#include "EC_Mesh.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgrePlaceable.h"

#include "MemoryLeakCheck.h"

#include <QDomDocument>
#include <QFile>
#include "RexTypes.h"

namespace Library
{

    LibraryModule::LibraryModule() :
    ModuleInterface(NameStatic()), 
    networkStateEventCategory_(0),
    networkInEventCategory_(0),
    frameworkEventCategory_(0),
    resource_event_category_(0),
    asset_event_category_(0),        
    library_widget_(0)    
    {
    }

    LibraryModule::~LibraryModule()
    {    
    }

    void LibraryModule::Load()
    {
    }

    void LibraryModule::PostInitialize()
    {
        frameworkEventCategory_ = framework_->GetEventManager()->QueryEventCategory("Framework");
        resource_event_category_ = framework_->GetEventManager()->QueryEventCategory("Resource");
        asset_event_category_ = framework_->GetEventManager()->QueryEventCategory("Asset");

        RegisterConsoleCommand(Console::CreateCommand("Library",
            "Shows web library.",
            Console::Bind(this, &LibraryModule::ShowWindow)));

        if (!library_widget_)
        {
            QGraphicsView *ui_view = framework_->GetUIView();
            library_widget_ = new LibraryWidget(ui_view);

            Foundation::UiServicePtr ui = framework_->GetService<Foundation::UiServiceInterface>(Foundation::Service::ST_Gui).lock();                
            UiProxyWidget *lib_proxy = ui->AddWidgetToScene(library_widget_);
            ui->RegisterUniversalWidget("Library", lib_proxy);

            connect(ui_view, SIGNAL(LibraryDropEvent(QDropEvent *) ), SLOT(LibraryDropEvent(QDropEvent *) ));
        }

    }

    void LibraryModule::Update(f64 frametime)
    {
    }

    bool LibraryModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface *data)
    {
        if (category_id == frameworkEventCategory_)
        {
            if (event_id == Foundation::NETWORKING_REGISTERED)
            {
                ProtocolUtilities::NetworkingRegisteredEvent *event_data = checked_static_cast<ProtocolUtilities::NetworkingRegisteredEvent *>(data);
                if (event_data)
                {
                    networkStateEventCategory_ = framework_->GetEventManager()->QueryEventCategory("NetworkState");
                    networkInEventCategory_ = framework_->GetEventManager()->QueryEventCategory("NetworkIn");
                    return false;
                }
            }

            if(event_id == Foundation::WORLD_STREAM_READY)
            {
                ProtocolUtilities::WorldStreamReadyEvent *event_data = checked_static_cast<ProtocolUtilities::WorldStreamReadyEvent *>(data);
                if (event_data)
                    currentWorldStream_ = event_data->WorldStream;

                networkInEventCategory_ = framework_->GetEventManager()->QueryEventCategory("NetworkIn");

                return false;
            }
        }

        if (category_id == asset_event_category_)
        {
            HandleAssetEvent(event_id, data);
        }

        if (category_id == resource_event_category_)
        {
            HandleResourceEvent(event_id, data);
        }

        return false;
    }

    bool LibraryModule::HandleAssetEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {

        if (event_id != Asset::Events::ASSET_READY)
            return false;

        Asset::Events::AssetReady *asset = checked_static_cast<Asset::Events::AssetReady*>(data); 
        if (!asset)
            return false;                

        if(asset->asset_type_ != "Unknown")
            return false;

        for(uint index = 0; index < scene_file_requests_.size(); index++)
        {
            if(scene_file_requests_.keys().at(index) == asset->tag_)
            {                   
                Foundation::AssetPtr assetPtr = asset->asset_;
                if (assetPtr)
                {                   
                    QString url = (scene_file_requests_.values().at(index)).toString();
                    int ind = url.lastIndexOf("/");
                    if (ind > 0)
                    {
                        //Base url
                        QString baseUrl = url.leftRef(ind).toString();                        
                        //File name                        
                        QString filename = url.rightRef(ind).toString();

                        Scene::ScenePtr scene = framework_->GetDefaultWorldScene();                        
                        LoadSceneFiles(QByteArray((const char*)assetPtr->GetData(), assetPtr->GetSize()), baseUrl);
                    }
                }
            }
        }
        
        return false;
    }

    bool LibraryModule::HandleResourceEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {

        if (event_id != Resource::Events::RESOURCE_READY)
            return false;
        
        Resource::Events::ResourceReady *res = dynamic_cast<Resource::Events::ResourceReady*>(data);
        assert(res);
        if (!res)
            return false;   

        if ( res->resource_->GetType() == "Mesh" || res->resource_->GetType() == "OgreMesh")
        {
            for(uint index = 0; index < mesh_file_requests_.size(); index++)
            {
                if(mesh_file_requests_.keys().at(index) == res->tag_)
                {                
                    
                    boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
                    if (!renderer)
                        return false;

                    QString url = (mesh_file_requests_.values().at(index)).toString();

                    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
                    if (!scene.get())
                        return false;

                    Scene::EntityPtr entity = scene->CreateEntity(framework_->GetDefaultWorldScene()->GetNextFreeId());

                    Foundation::ComponentInterfacePtr place = entity->GetOrCreateComponent("EC_OgrePlaceable");
                    Foundation::ComponentInterfacePtr mesh = entity->GetOrCreateComponent("EC_Mesh");

                    if (mesh)
                    {
                        EC_Mesh *my_mesh = dynamic_cast<EC_Mesh*>(mesh.get());
                        if (my_mesh)
                        {                            
                            my_mesh->SetMesh(url);                            
                        }
                        OgreRenderer::EC_OgrePlaceable *my_place = dynamic_cast<OgreRenderer::EC_OgrePlaceable*>(place.get());
                        if (my_place)
                        {                                                        
                            my_place->SetPosition(raycast_pos_);
                        }
                        
                    }
                }
            }
        }

        return false;
    }

    void LibraryModule::LoadSceneFiles(const QByteArray &bytearr, QString baseUrl)
    {
        QDomDocument scene_doc("Scene");

        if (!scene_doc.setContent(bytearr))
            return;

        // Check for existence of the scene element before we begin
        QDomElement scene_elem = scene_doc.firstChildElement("scene");
        if (scene_elem.isNull())
            return;

        QDomNodeList entities = scene_doc.elementsByTagName("entity");
        if (entities.isEmpty())
            return;

        for (int i = 0; i < entities.count(); i++)
        {
            QDomNode node = entities.item(i);
            while (!node.isNull()) {
                if (node.isElement()) {
                    QDomElement e = node.toElement();
                    QString meshFile = e.attribute("meshFile");
                    if (!meshFile.isEmpty())
                    {
                        baseUrl.append("/"+meshFile);

                        boost::shared_ptr<OgreRenderer::OgreRenderingModule> rendering_module = 
                            framework_->GetModuleManager()->GetModule<OgreRenderer::OgreRenderingModule>().lock();

                        if (!rendering_module)
                            return;

                        OgreRenderer::RendererPtr renderer = rendering_module->GetRenderer();
                        
                        request_tag_t tag = renderer->RequestResource(baseUrl.toStdString(), OgreRenderer::OgreMeshResource::GetTypeStatic());
                        if (tag)
                            mesh_file_requests_.insert(tag, baseUrl);
                                            
                    }
                }
                node = node.nextSibling();
            }
        }
    }

    void LibraryModule::LibraryDropEvent(QDropEvent *drop_event)
    {
        if (drop_event->mimeData()->hasUrls()) 
        {
            foreach (QUrl url, drop_event->mimeData()->urls())
            {                
                if (url.toString().endsWith(".scene"))
                {
                    //Do raycast for mouse position for scene file
                    boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
                    if (!renderer)
                        return;

                    Foundation::RaycastResult cast_result = renderer->Raycast(drop_event->pos().x(), drop_event->pos().y());
                    raycast_pos_ = cast_result.pos_;
                    Scene::Entity *entity = cast_result.entity_;
                    if (!entity) // User didn't click on terrain or other entities.
                        return;

                    //Emit signal to be catched in Python module
                    emit UploadSceneFile(url.toString(), cast_result.pos_.x, cast_result.pos_.y, cast_result.pos_.z);


                    //Request scene file
                    Foundation::AssetServiceInterface *asset_service = framework_->GetService<Foundation::AssetServiceInterface>();
                    if (asset_service) 
                    {                        
                        request_tag_t tag = asset_service->RequestAsset(url.toString().toStdString(), RexTypes::ASSETTYPENAME_UNKNOWN);
                        if (tag)
                            scene_file_requests_.insert(tag, url);
                    }                    
                }
            }
        }
    }


Console::CommandResult LibraryModule::ShowWindow(const StringVector &params)
{

    if (!library_widget_)
    {
        QGraphicsView *ui_view = framework_->GetUIView();
        library_widget_ = new LibraryWidget(ui_view);

        Foundation::UiServicePtr ui = framework_->GetService<Foundation::UiServiceInterface>(Foundation::Service::ST_Gui).lock();                
        ui->AddWidgetToScene(library_widget_);

        connect(ui_view, SIGNAL(LibraryDropEvent(QDropEvent *) ), SLOT(LibraryDropEvent(QDropEvent *) ));
    }

    library_widget_->show();

    return Console::ResultSuccess("Library widget initialized.");

}

const std::string LibraryModule::moduleName = std::string("LibraryModule");

const std::string &LibraryModule::NameStatic()
{
    return moduleName;
}

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

}

using namespace Library;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
POCO_EXPORT_CLASS(LibraryModule)
POCO_END_MANIFEST
