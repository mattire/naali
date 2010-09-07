/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   LibraryModule.h
 *  @brief  
 */

#ifndef incl_LibraryModule_h
#define incl_LibraryModule_h

#include "LibraryModuleApi.h"
#include "ModuleInterface.h"
#include "ModuleLoggingFunctions.h"
#include "Vector3D.h"

#include <QDropEvent>
#include <QObject>

class RexUUID;

namespace Foundation
{
    class EventDataInterface;
    class AssetInterface;
}

namespace ProtocolUtilities
{
    class ProtocolModuleInterface;
    class WorldStream;
    class NetworkEventInboundData;
    class NetInMessage;
    typedef boost::weak_ptr<ProtocolModuleInterface> ProtocolWeakPtr;
    typedef boost::shared_ptr<WorldStream> WorldStreamPtr;
}

namespace Library
{
    class LibraryWidget;
}

class UiProxyWidget;

namespace Library
{
    class LIBRARY_MODULE_API LibraryModule :  public QObject, public Foundation::ModuleInterface
    {
        Q_OBJECT

    public:
        /// Default constructor.
        LibraryModule();

        /// Destructor 
        ~LibraryModule();

        /// ModuleInterfaceImpl overrides.
        void Load();
        void PostInitialize();
        void Update(f64 frametime);
        bool HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data);

        //! Handles an asset event. Called from LibraryModule.
        bool HandleAssetEvent(event_id_t event_id, Foundation::EventDataInterface* data);

        //! Show the library widget
        Console::CommandResult ShowWindow(const StringVector &params);

        MODULE_LOGGING_FUNCTIONS

        /// Returns name of this module. Needed for logging.
        static const std::string &NameStatic();

        /// Name of this module.
        static const std::string moduleName;

    private:
        Q_DISABLE_COPY(LibraryModule);

        /// NetworkState event category.
        event_category_id_t networkStateEventCategory_;

        /// NetworkIn event category.
        event_category_id_t networkInEventCategory_;

        /// Framework event category
        event_category_id_t frameworkEventCategory_;

        //! Id for Resource event category
        event_category_id_t resource_event_category_;

        //! Id for Asset event category
        event_category_id_t asset_event_category_;

        //! WorldStream will handle those network messages that we are wishing to send.
        ProtocolUtilities::WorldStreamPtr currentWorldStream_;

        /// LibraryWidget pointer
        Library::LibraryWidget *library_widget_;        

        //! Asset_tags for map image requests.
        std::vector<request_tag_t> scene_file_requests_;

    private slots:
        void LibraryDropEvent(QDropEvent *drop_event);


    signals:
        void UploadSceneFile(QString url, Vector3df position);

   };
}

#endif
