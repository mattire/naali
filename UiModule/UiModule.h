// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_UiModule_h
#define incl_UiModule_UiModule_h

#include "ModuleInterface.h"
#include "ModuleLoggingFunctions.h"

#include "UiModuleApi.h"
#include "UiModuleFwd.h"
#include "UiTypes.h"

#include <QObject>
#include <QMap>
#include <QPair>
#include <QStringList>

class KeyEvent;
class InputContext;

namespace ProtocolUtilities
{
    class WorldStream;
}

namespace UiServices
{
    class UiSettingsService;
    class UiSceneService;
    class MessageNotification;

    typedef boost::shared_ptr<UiSettingsService> UiSettingsPtr;
    typedef boost::shared_ptr<UiSceneService> UiSceneServicePtr;

    //! UiModule provides user interface services
    /*! For details about Inworld Widget Services read UiWidgetServices.h
     *  For details about Notification Services read UiNotificationServices.h
     *  Include above headers into your .cpp and UiServicesFwd.h to your .h files for easy access
     *  For the UI services provided for other, see @see UiSceneService
     */
    class UI_MODULE_API UiModule : public QObject, public Foundation::ModuleInterface
    {
        Q_OBJECT

    public:
        UiModule();
        ~UiModule();

        /*************** ModuleInterfaceImpl ***************/
        void Load();
        void Unload();
        void Initialize();
        void PostInitialize();
        void Uninitialize();
        void Update(f64 frametime);
        bool HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data);

        /*************** UiModule Services ***************/

        //! InworldSceneController will give you a QObject derived class that will give you all
        //! the UI related services like adding your own QWidgets into the 2D scene 
        //! \return InworldSceneController The scene manager with scene services
        InworldSceneController *GetInworldSceneController() const { return inworld_scene_controller_; }

        NotificationManager *GetNotificationManager() const { return inworld_notification_manager_; }

        CoreUi::UiStateMachine *GetUiStateMachine() const { return ui_state_machine_; }

        Ether::Logic::EtherLoginNotifier *GetEtherLoginNotifier() const;

        //! Logging
        MODULE_LOGGING_FUNCTIONS;

        //! Returns name of this module.
        static const std::string &NameStatic() { return type_name_static_; }

    private slots:
        void OnKeyPressed(KeyEvent *key);

        void OnSceneChanged(const QString &old_name, const QString &new_name);

        //! Takes UI screenshots of world/avatar with rendering service
        void TakeEtherScreenshots();

    private:
        //! Notify all ui module components of connected/disconnected state
        //! \param message Optional message, e.g. error message.
        void PublishConnectionState(ConnectionState connection_state, const QString &message = "");

        //! Get all the category id:s of categories in eventQueryCategories
        void SubscribeToEventCategories();

        //! Type name of this module.
        static std::string type_name_static_;

        //! Current query categories
        QStringList event_query_categories_;

        //! Current subscribed category events
        QMap<QString, event_category_id_t> service_category_identifiers_;

        //! Pointer to the QOgre UiView
        QGraphicsView *ui_view_;

        //! UiStateMachine pointer
        CoreUi::UiStateMachine *ui_state_machine_;

        //! Service getter, provides functions to access Naali services
        CoreUi::ServiceGetter *service_getter_;

        //! InworldSceneController pointer
        InworldSceneController *inworld_scene_controller_;

        //! NotificationManager pointer
        NotificationManager *inworld_notification_manager_;

        //! Ether Logic
        Ether::Logic::EtherLogic *ether_logic_;

        //! Current World Stream pointer
        boost::shared_ptr<ProtocolUtilities::WorldStream> current_world_stream_;

        //! Ui settings service 
        UiSettingsPtr ui_settings_service_;

        //! Ui service.
        UiSceneServicePtr ui_scene_service_;

        //! Input context for Ether
        boost::shared_ptr<InputContext> input;

        //! Welcome message to be sent when inworld scene is enabled
        //! Do NOT delete this on deconstructor or anywhere else for that matter!
        MessageNotification *welcome_message_;
    };
}

#endif // incl_UiModule_UiModule_h
