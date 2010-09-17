// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_WorldBuildingModule_BuildSceneManager_h
#define incl_WorldBuildingModule_BuildSceneManager_h

#include "ForwardDefines.h"
#include "WorldBuildingServiceInterface.h"

#include "WorldObjectView.h"
#include "BuildingWidget.h"
#include "BuildToolbar.h"

#include "PythonHandler.h"
#include "CameraHandler.h"

#include "ui_ObjectInfoWidget.h"
#include "ui_ObjectManipulationsWidget.h"

#include <QtInputKeyEvent.h>
#include <QObject>
#include <QTimer>
#include <QPair>
#include <QDropEvent>

class QtAbstractPropertyBrowser;
class QtProperty;

namespace Ui
{
    class BuildinWidget;
}

namespace CoreUi
{
    class UiStateMachine;
}

namespace WorldBuilding
{
    class BuildScene;
    class AnchorLayout;
    class PropertyEditorHandler;
    class WorldObjectView;

    namespace Helpers
    {
        class UiHelper;
    }

    typedef CoreUi::UiStateMachine StateMachine;
    typedef QPair<QWidget*,QGraphicsProxyWidget*> TransferPair;

    class BuildSceneManager : public Foundation::WorldBuildingServiceInterface
    {
        
    Q_OBJECT
    
    public:
        BuildSceneManager(QObject *parent, Foundation::Framework *framework);
        virtual ~BuildSceneManager();

        bool inworld_state;

    public slots:
        //! Cleans the passed python widgets from our layout
        void CleanPyWidgets();
        bool IsBuildingActive();

        //! Handle our key context input
        void KeyPressed(KeyEvent *key);
        void KeyReleased(KeyEvent *key);

        //! Public functions called by WorldBuildingModule
        void ToggleBuildScene();
        void ShowBuildScene();
        void HideBuildScene();

        void CreateCamera();
        void ResetCamera();
        void ResetEditing();

        void ObjectSelected(Scene::Entity *entity);
        void ObjectDeselected();
        void UpdateObjectViewport();
        
        void RotateObject(qreal x, qreal y);
        void Zoom(qreal delta);

        void HandleCreatedEntities();

        // WorldBuildingServiceInterface
        virtual QObject *GetPythonHandler() const;

        //! Handle primitive EntityCreated
        void EntityCreated(Scene::Entity* entity, AttributeChange::Type change);

    private slots:
        void InitScene();
        void SceneChangedNotification(const QString &old_name, const QString &new_name);
        void ObjectSelected(bool selected);

        void ModeToggleMove();
        void ModeToggleScale();
        void ModeToggleRotate();
        void NewObjectClicked();
        void DuplicateObjectClicked();
        void DeleteObjectClicked();

        void ManipModeChanged(PythonParams::ManipulationMode mode);

        void HandleWidgetTransfer(const QString &name, QGraphicsProxyWidget *widget);
        void HandleTransfersBack();
        void HandlePythonWidget(const QString &type, QWidget *widget);

        void ToggleLights();

        void LibraryDropEvent(QDropEvent *drop_event);

    private:
        Foundation::Framework *framework_;
        QString scene_name_;

        BuildScene *scene_;
        AnchorLayout *layout_;

        WorldObjectView* world_object_view_;
        Ui::BuildingWidget *object_info_widget_;
        Ui::BuildingWidget *object_manipulations_widget_;
        Ui::BuildToolbar *toolbar_;

        Ui_ObjectInfoWidget object_info_ui;
        Ui_ObjectManipulationsWidget object_manip_ui;

        Helpers::UiHelper *ui_helper_;
        PythonHandler *python_handler_;
        
        PropertyEditorHandler *property_editor_handler_;
        Scene::Entity* selected_entity_;

        View::CameraHandler *camera_handler_;
        View::CameraID selected_camera_id_;

        QList<QWidget*> toggle_visibility_widgets_;
        QList<QWidget*> python_deleted_widgets_;

        QTimer *viewport_poller_;
        bool override_server_time_;
        bool prim_selected_;

        QMap<QString, TransferPair > tranfer_widgets_;

        QMap<QUrl, Vector3df> mesh_file_requests_;
        Vector3df raycast_pos_;
        QList<Scene::Entity *> created_entities_;


    signals:
        void UploadSceneFile(QString url, int x, int y, int z);

    };
}

#endif