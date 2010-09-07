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

#include "Renderer.h"

#include "MemoryLeakCheck.h"

namespace Library
{

    LibraryModule::LibraryModule() :
        ModuleInterface(NameStatic()), 
        networkStateEventCategory_(0),
        networkInEventCategory_(0),
        frameworkEventCategory_(0),
        resource_event_category_(0),
        asset_event_category_(0),
        scene_file_requests_(0),
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
        //    HandleAssetEvent(event_id, data);
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

        //if(asset->asset_type_ != RexTypes::ASSETTYPENAME_SCENE)
        //    return false;

        for(uint index = 0; index < scene_file_requests_.size(); index++)
        {
            if(scene_file_requests_.at(index) == asset->tag_)
            {   
                
                //TODO If decided to download and use in drag and drop in the future.

            }
        }

        return false;
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
                    Scene::Entity *entity = cast_result.entity_;
                    if (!entity) // User didn't click on terrain or other entities.
                        return;

                    //Emit signal to be catched in Python module
                    emit UploadSceneFile(url.toString(), cast_result.pos_);

                    //Not used now
                    //Foundation::AssetServiceInterface *asset_service = framework_->GetService<Foundation::AssetServiceInterface>();
                    //if (asset_service) 
                    //{                        
                    //    request_tag_t tag = asset_service->RequestAsset(url.toString().toStdString(), ASSETTYPENAME_SCENE);
                    //    if (tag)
                    //        scene_file_requests_.push_back(tag);                
                    //}                    
                }
            }
        }
    }


    Console::CommandResult LibraryModule::ShowWindow(const StringVector &params)
    {

        QGraphicsView *ui_view = framework_->GetUIView();
        library_widget_ = new LibraryWidget(ui_view);

        Foundation::UiServicePtr ui = framework_->GetService<Foundation::UiServiceInterface>(Foundation::Service::ST_Gui).lock();                
        ui->AddWidgetToScene(library_widget_);

        library_widget_->show();

        connect(ui_view, SIGNAL(LibraryDropEvent(QDropEvent *) ), SLOT(LibraryDropEvent(QDropEvent *) ));

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
