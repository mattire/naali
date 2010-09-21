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
#include "EC_OgreMesh.h"
#include "EC_OpenSimPrim.h"
#include "EntityComponent/EC_NetworkPosition.h"
#include "MemoryLeakCheck.h"
#include "RexLogicModule.h"

#include <QDomDocument>
#include <QFile>
#include "RexTypes.h"

#include "ScriptServiceInterface.h" // call py directly

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
            ui->RegisterUniversalWidget("library", lib_proxy);

            //connect(ui_view, SIGNAL(LibraryDropEvent(QDropEvent *)), SLOT(LibraryDropEvent(QDropEvent *) ));

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
        
        //if (category_id == asset_event_category_)
        //{
        //    HandleAssetEvent(event_id, data);
        //}

        //if (category_id == resource_event_category_)
        //{
        //    HandleResourceEvent(event_id, data);
        //}

        return false;
    }

    //bool LibraryModule::HandleAssetEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    //{

    //    if (event_id != Asset::Events::ASSET_READY)
    //        return false;

    //    Asset::Events::AssetReady *asset = checked_static_cast<Asset::Events::AssetReady*>(data); 
    //    if (!asset)
    //        return false;                

    //    if(asset->asset_type_ != "Unknown")
    //        return false;

    //    for(uint index = 0; index < scene_file_requests_.size(); index++)
    //    {
    //        if(scene_file_requests_.keys().at(index) == asset->tag_)
    //        {                   
    //            Foundation::AssetPtr assetPtr = asset->asset_;
    //            if (assetPtr)
    //            {                   
    //                QString url = (scene_file_requests_.values().at(index)).toString();
    //                int ind = url.lastIndexOf("/");
    //                if (ind > 0)
    //                {
    //                    //Base url
    //                    QString baseUrl = url.leftRef(ind).toString();                        
    //                    //File name                        
    //                    QString filename = url.rightRef(ind).toString();

    //                    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();                        
    //                    LoadSceneFiles(QByteArray((const char*)assetPtr->GetData(), assetPtr->GetSize()), baseUrl);
    //                }
    //            }
    //        }
    //    }
    //    
    //    return false;
    //}

    //bool LibraryModule::HandleResourceEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    //{

    //    if (event_id != Resource::Events::RESOURCE_READY)
    //        return false;
    //    
    //    Resource::Events::ResourceReady *res = dynamic_cast<Resource::Events::ResourceReady*>(data);
    //    assert(res);
    //    if (!res)
    //        return false;   

    //    if ( res->resource_->GetType() == "Mesh" || res->resource_->GetType() == "OgreMesh")
    //    {
    //        for(uint index = 0; index < mesh_file_requests_.size(); index++)
    //        {
    //            if(mesh_file_requests_.keys().at(index) == res->tag_)
    //            {                
    //                
    //                boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
    //                if (!renderer)
    //                    return false;

    //                QString url = (mesh_file_requests_.values().at(index)).toString();

    //                Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    //                if (!scene.get())
    //                    return false;


    //                currentWorldStream_->SendObjectAddPacket(raycast_pos_);

    //                //Scene::EntityPtr entity = scene->CreateEntity(framework_->GetDefaultWorldScene()->GetNextFreeId());

    //                //Foundation::ComponentInterfacePtr place = entity->GetOrCreateComponent("EC_OgrePlaceable");
    //                //Foundation::ComponentInterfacePtr mesh = entity->GetOrCreateComponent("EC_Mesh");

    //                //if (mesh)
    //                //{
    //                //    EC_Mesh *my_mesh = dynamic_cast<EC_Mesh*>(mesh.get());
    //                //    if (my_mesh)
    //                //    {                            
    //                //        my_mesh->SetMesh(url); 
    //                //        uint submesh_count = 0;
    //                //        submesh_count = my_mesh->GetNumMaterials();
    //                //    }
    //                //    OgreRenderer::EC_OgrePlaceable *my_place = dynamic_cast<OgreRenderer::EC_OgrePlaceable*>(place.get());
    //                //    if (my_place)
    //                //    {                                                        
    //                //        my_place->SetPosition(raycast_pos_);
    //                //    }
    //                //    
    //                //}
    //            }
    //        }
    //    }

    //    return false;
    //}

    void LibraryModule::LibraryDropEvent(QDropEvent *drop_event)
    {
        if (drop_event->mimeData()->hasUrls()) 
        {
            foreach (QUrl url, drop_event->mimeData()->urls())
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

                if (url.toString().endsWith(".scene"))
                {
                    //Emit signal to be catched in Python module
                    //emit UploadSceneFile(url.toString(), cast_result.pos_.x, cast_result.pos_.y, cast_result.pos_.z);
                    //Call python directly (dont want to add dependency to optional module in pythonscript module)
                    //Change coords to string format for calling py
                    QString qx;
                    qx.append(QString("%1").arg(cast_result.pos_.x));
                    QString qy;
                    qy.append(QString("%1").arg(cast_result.pos_.y));
                    QString qz;
                    qz.append(QString("%1").arg(cast_result.pos_.z));

                    Foundation::ServiceManagerPtr manager = this->framework_->GetServiceManager();
                    boost::shared_ptr<Foundation::ScriptServiceInterface> pyservice = manager->GetService<Foundation::ScriptServiceInterface>(Foundation::Service::ST_PythonScripting).lock();
                    if (pyservice)
                        pyservice->RunString("import localscene; lc = localscene.getLocalScene(); lc.onUploadSceneFile('" + 
                                              url.toString() + "', " + 
                                              qx +", " + 
                                              qy + ", " + 
                                              qz + "');");

                }
                else if (url.toString().endsWith(".mesh"))
                {

                    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
                    connect(scene.get(), SIGNAL(EntityCreated(Scene::Entity *, AttributeChange::Type)), SLOT(EntityCreated(Scene::Entity *, AttributeChange::Type)));

                    currentWorldStream_->SendObjectAddPacket(raycast_pos_);
                    mesh_file_requests_.insert(url, raycast_pos_);

                    //connect(currentWorldStream_.get(), SIGNAL(EntityCreated(Scene::Entity *, AttributeChange::Type)), SLOT(EntityCreated(Scene::Entity *, AttributeChange::Type)));

                    //Request mesh file           
                    //boost::shared_ptr<OgreRenderer::OgreRenderingModule> rendering_module = 
                    //framework_->GetModuleManager()->GetModule<OgreRenderer::OgreRenderingModule>().lock();

                    //if (!rendering_module)
                    //    return;

                    //OgreRenderer::RendererPtr renderer = rendering_module->GetRenderer();
                    //
                    //request_tag_t tag = renderer->RequestResource(url.toString().toStdString(), OgreRenderer::OgreMeshResource::GetTypeStatic());
                    //if (tag)
                    //    mesh_file_requests_.insert(tag, url);
                    
                }
            }
        }
    }

    void LibraryModule::EntityCreated(Scene::Entity* entity, AttributeChange::Type change)
    {
        EC_OpenSimPrim *prim = entity->GetComponent<EC_OpenSimPrim>().get();
        RexLogic::EC_NetworkPosition *pos = entity->GetComponent<RexLogic::EC_NetworkPosition>().get();
        if (!pos)
            return;

        QVector3D temp = pos->GetQPosition();

        for(uint index = 0; index < mesh_file_requests_.size(); index++)
        {            
            Vector3df meshpos = mesh_file_requests_.values().at(index);
            if(meshpos.x == temp.x() && meshpos.y == temp.y())
            {
                QUrl url = mesh_file_requests_.keys().at(index);
                Foundation::ComponentInterfacePtr comp = entity->GetOrCreateComponent(OgreRenderer::EC_OgreMesh::TypeNameStatic());

                OgreRenderer::EC_OgreMesh *mesh = dynamic_cast<OgreRenderer::EC_OgreMesh*>(comp.get());
                if (mesh)
                {                   
                    mesh->SetMesh(url.toString().toStdString());
                    prim->setMeshID(url.toString());
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
