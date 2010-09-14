// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_WorldBuildingModule_UiHelper_h
#define incl_WorldBuildingModule_UiHelper_h

#include <EC_OpenSimPrim.h>

#include "Foundation.h"

#include <QSignalMapper>
#include <QObject>
#include <QString>
#include <QMap>

#include <QtVariantPropertyManager>
#include <QtVariantEditorFactory>
#include <QtTreePropertyBrowser>
#include <QtVariantProperty>

#include "CustomLineEditFactory.h"

#include "PythonHandler.h"
#include "AnchorLayout.h"
#include "BuildingWidget.h"
#include "ui_ObjectManipulationsWidget.h"
#include "ui_ObjectInfoWidget.h"

namespace WorldBuilding
{
    namespace Helpers
    {
        class UiHelper : public QObject
        {

        Q_OBJECT

        public:
            // Constructor
            UiHelper(QObject *parent, Foundation::Framework *framework);

            // Public containers
            QMap<QString, QtProperty*> editor_items;
            QStringList information_items, rex_prim_data_items, object_shape_items;

            // Public pointers
            QtVariantPropertyManager *variant_manager;
            QtStringPropertyManager *string_manager;
            QtTreePropertyBrowser *browser;

        public slots:
            QString CheckUiValue(QString value);
            QString CheckUiValue(unsigned int value);
            void SetManipMode(PythonParams::ManipulationMode mode) { manip_mode_ = mode; }

            QtTreePropertyBrowser *CreatePropertyBrowser(QWidget *parent, QObject *controller, EC_OpenSimPrim *prim);

            void SetupManipControls(Ui_ObjectManipulationsWidget *manip_ui, QObject *python_handler);

            void SetRotateValues(int x, int y, int z);
            void RotateXChanged(int value);
            void RotateYChanged(int value);
            void RotateZChanged(int value);

            void SetScaleValues(double x, double y, double z);
            void OnScaleChanged(double value);

            void SetPosValues(double x, double y, double z);
            void OnPosChanged(double value);

            void SetupVisibilityButtons(AnchorLayout *layout, Ui::BuildingWidget *manip_ui, Ui::BuildingWidget *info_ui);
            void AddBrowsePair(QString name, QPushButton *button, QWidget *tool_widget);

        private slots:
            void CollapseSubGroups(QtBrowserItem *main_group);

            QtProperty *CreateInformationGroup(QtVariantPropertyManager *variant_manager, EC_OpenSimPrim *prim);
            QtProperty *CreateRexPrimDataGroup(QtVariantPropertyManager *variant_manager, EC_OpenSimPrim *prim);
            QtProperty *CreateShapeGroup(QtVariantPropertyManager *variant_manager, EC_OpenSimPrim *prim);

            void BrowseClicked(QWidget *widget_ptr);
            void BrowserAndUpload(QString category, QString filter, QString upload_to, QWidget *tool_widget);
            void AssetUploadCompleted(const QString &filename, const QString &asset_ref);

        signals:
            void PosChanged(double x, double y, double z);
            void ScaleChanged(double x, double y, double z);
            void RotationChanged(int x, int y, int z);

        private:
            QSignalMapper *mapper_;
            QStringList shape_limiter_zero_to_one; 
            QStringList shape_limiter_minushalf_to_plushalf;
            QStringList shape_limiter_minusone_to_plusone;
            QStringList shape_limiter_zero_to_three;

            QObject *python_handler_;
            Ui_ObjectManipulationsWidget *manip_ui_;

            bool ignore_manip_changes_;

            PythonParams::ManipulationMode manip_mode_;

            QMap<QString, QPair<QPushButton*,QWidget*> > browser_pairs_;

            Foundation::Framework *framework_;

            QMap<QString, QPair<QWidget*, QString> > pending_uploads_;
        };
    }
}

#endif