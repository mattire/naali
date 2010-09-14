// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_PythonScriptModule_PythonScriptModule_h
#define incl_PythonScriptModule_PythonScriptModule_h

#include "Foundation.h"
#include "ModuleInterface.h"
#include "ModuleLoggingFunctions.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QVariantMap>

#ifdef PYTHON_FORCE_RELEASE_VERSION
  #ifdef _DEBUG
    #undef _DEBUG
    #include <Python.h>
    #define _DEBUG
  #else
    #include <Python.h>
  #endif 
#else
    #include <Python.h>
#endif

class EC_OpenSimPrim;

namespace OgreRenderer
{
    class Renderer;
}

namespace Foundation
{
    class WorldLogicInterface;
}

namespace ProtocolUtilities
{
    class InventorySkeleton;
    class WorldStream;
    typedef boost::shared_ptr<WorldStream> WorldStreamPtr;
}

namespace MediaPlayer
{
    class ServiceInterface;
}

typedef boost::shared_ptr<ProtocolUtilities::InventorySkeleton> InventoryPtr;

namespace PythonScript
{
    class PythonEngine;
    typedef boost::shared_ptr<PythonEngine> PythonEnginePtr;

    //! A scripting module using Python
    class MODULE_API PythonScriptModule : public QObject, public Foundation::ModuleInterface
    {
        Q_OBJECT

    public slots: //things for the py side to call.
        OgreRenderer::Renderer* GetRenderer() const;
        Foundation::WorldLogicInterface* GetWorldLogic() const;
        Scene::SceneManager* GetScene(const QString &name) const;
        void RunJavascriptString(const QString &codestr, const QVariantMap &context = QVariantMap());
        InputContext* GetInputContext() const { return input.get(); }
        InputContext* CreateInputContext(const QString &name, int priority = 100);
        MediaPlayer::ServiceInterface* GetMediaPlayerService() const;

        /**
        */
        void RunScript(const QString &filename);

    public:
        PythonScriptModule();
        virtual ~PythonScriptModule();

        // Module interface overrides
        virtual void Load();
        virtual void Unload();
        virtual void Initialize();
        virtual void PostInitialize();
        virtual void Uninitialize();
        virtual void Update(f64 frametime);
        virtual bool HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data);

        //! callback for console command
        Console::CommandResult ConsoleRunString(const StringVector &params);
        Console::CommandResult ConsoleRunFile(const StringVector &params);
        Console::CommandResult ConsoleReset(const StringVector &params);

        MODULE_LOGGING_FUNCTIONS

        //! returns name of this module. Needed for logging.
        static const std::string &NameStatic() { return type_name_static_; }

        static void Add3DCanvasComponents(Scene::Entity *entity, QWidget *widget, const QList<uint> &submeshes, int refresh_rate);

        //Foundation::Framework* GetFramework() { return frameworkptr;  };//this still returns null or 0... WHY?
        //static Foundation::ScriptEventInterface* engineAccess;

        //api code is outside the module now, but reuses these .. err, but can't see 'cause dont have a ref to the instance?
        // Category id for incoming messages.
        event_category_id_t inboundCategoryID_;
        event_category_id_t inputeventcategoryid;
        event_category_id_t networkstate_category_id;
        event_category_id_t framework_category_id;
        event_category_id_t scene_event_category_ ;

        /// Returns the currently initialized PythonScriptModule.
        static PythonScriptModule *GetInstance();

        Scene::ScenePtr GetScenePtr() const { return framework_->GetDefaultWorldScene(); }
        PyObject* WrapQObject(QObject* qobj) const;

        PyObject* entity_create(entity_id_t ent_id); //, Scene::EntityPtr entity);

//        PyTypeObject *GetRexPyTypeObject();

        // Inventory skeleton retrieved during login process
        InventoryPtr inventory;

        /// World stream pointer.
        ProtocolUtilities::WorldStreamPtr worldstream;

    private:
        //! Type name of the module.
        static std::string type_name_static_;

        //! Static instance of ourselves.
        static PythonScriptModule *pythonScriptModuleInstance_;

        PythonEnginePtr engine_;
        bool pythonqt_inited;

        //a testing place
        void x();

        PyObject *apiModule; //the module made here that exposes the c++ side / api, 'rexviewer'

        // the hook to the python-written module manager that passes events on
        PyObject *pmmModule, *pmmDict, *pmmClass, *pmmInstance;
        PyObject *pmmArgs, *pmmValue;

        //Foundation::ScriptObject* modulemanager;
        
        // can't get passing __VA_ARGS__ to pass my args 
        //   in PythonScriptObject::CallMethod2
        //   so reverting to use the Py C API directly, not using the ScriptObject now
        //   for the modulemanager 
        
        // EventManager to member variable to be accessed from SubscribeNetworkEvents()
        Foundation::EventManagerPtr em_;

        /// The default input context for python code to access. This context operates below
        /// the Qt windowing priority.
        InputContextPtr input;

        QList<InputContextPtr> created_inputs_;

    private slots:
        /** Called when new component is added to the active scene.
            Currently used for handling EC_Script.
            @param entity Entity for which the component was added.
            @param component The added component.
         */
        void OnComponentAdded(Scene::Entity *entity, Foundation::ComponentInterface *component);

        /** Called when component is removed from the active scene.
            Currently used for handling EC_Script.
            @param entity Entity from which the component was removed.
            @param component The removed component.
        */
        void OnComponentRemoved(Scene::Entity *entity, Foundation::ComponentInterface *component);
    };

    static PythonScriptModule *self() { return PythonScriptModule::GetInstance(); }
}

#endif
