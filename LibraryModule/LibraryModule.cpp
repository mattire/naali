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
#include "OgreRenderingModule.h"
#include "RexLogicModule.h"

#include "Renderer.h"
#include "Ogre.h"
#include "SceneEvents.h"
#include "SceneManager.h"
#include "UiProxyWidget.h"

#include "MemoryLeakCheck.h"

namespace Library
{

    LibraryModule::LibraryModule() :
    ModuleInterface(NameStatic()), 
    networkStateEventCategory_(0),
    networkInEventCategory_(0),
    frameworkEventCategory_(0),
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

            connect(ui_view, SIGNAL(LibraryDropEvent(QDropEvent *)), SLOT(LibraryDropEvent(QDropEvent *) ));
            Scene::ScenePtr scene = framework_->GetDefaultWorldScene();

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
                    emit UploadSceneFile(url.toString(), cast_result.pos_.x, cast_result.pos_.y, cast_result.pos_.z);

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
