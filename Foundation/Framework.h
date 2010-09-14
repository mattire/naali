// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Foundation_Framework_h
#define incl_Foundation_Framework_h

// Application name is statically defined here
#define APPLICATION_NAME "realXtend"

#include "EventDataInterface.h"
#include "Profiler.h"
#include "ModuleManager.h"
#include "ServiceManager.h"

#include <boost/smart_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/timer.hpp>

class QApplication;
class QGraphicsView;
class QWidget;
class QObject;
class InputServiceInterface;

namespace Poco
{
    class SplitterChannel;
    class Channel;
    class Formatter;
}

namespace Foundation
{
    class FrameworkQtApplication;
    class KeyStateListener;
    class MainWindow;

    //! contains entry point for the framework.
    /*! Allows access to various managers and services. The standard way of using
        the framework is by first creating the framework and then calling Framework::Go()
        which will then load / initialize all modules and enters the main loop which
        automatically updates all loaded modules.

        There are other ways of using the framework. To f.ex. run without the main loop,
        see Framework::PostInitialize(). All the modules need to be updated manually then.

        The constructor initalizes the framework. Config or logging should not be used
        usually without first initializing the framework.

        \ingroup Foundation_group
    */
    class Framework : public QObject
    {
        Q_OBJECT

    public:
        typedef std::map<std::string, Scene::ScenePtr> SceneMap;

        //! constructor. Initializes the framework.
        Framework(int argc, char** argv);

        //! destructor
        ~Framework();

        //! Parse program options from command line arguments
        /*! For internal use. Should be called immediatelly after creating the framework,
            so all options will be taken in effect properly.
        */
        void ParseProgramOptions();

        //! Do post-initialization steps. No need to call if using Framework::Go().
        /*! This function can be used if you wish to use the framework without main loop.
            It does
            In that case the correct order is:
                Foundation::Framework fw;                  // create the framework
                fw.GetModuleManager()->ExcludeModule(...)  // optional step for excluding certain modules
                ...                                        // other initalization steps
                fw.PostInitialize()
                ...                                        // continue program execution without framework's main loop
        */
        void PostInitialize();

        //! Entry point for the framework.
        void Go();

        //! Runs through a single frame of logic update and rendering.
        void ProcessOneFrame();

        //! Returns component manager.
        ComponentManagerPtr GetComponentManager() const;

        //! Returns module manager.
        ModuleManagerPtr GetModuleManager() const;

        //! Returns service manager.
        ServiceManagerPtr GetServiceManager() const;

        //! Returns event manager.
        EventManagerPtr GetEventManager() const;

        //! Returns platform abstraction object.
        PlatformPtr GetPlatform() const;

        //! Returns config manager.
        ConfigurationManagerPtr GetConfigManager();

        //! Returns thread task manager.
        ThreadTaskManagerPtr GetThreadTaskManager();

        //! Signal the framework to exit
        void Exit();

        //! Cancel a pending exit
        void CancelExit();

        //! Force immediate exit, with no possibility to cancel it
        void ForceExit();

        //! Returns true if framework is in the process of exiting (will exit at next possible opportunity)
        bool IsExiting() const { return exit_signal_; }

        //! Returns true if framework is properly initialized and Go() can be called.
        bool Initialized() const { return initialized_; }

        //! Returns the default configuration
        ConfigurationManager &GetDefaultConfig();

        //! Returns pointer to the default configuration
        ConfigurationManager *GetDefaultConfigPtr();

        //! Shortcut for retrieving a service. See ServiceManager::GetService() for more info
        template <class T>
        __inline boost::weak_ptr<T> GetService(service_type_t type) { return service_manager_->GetService<T>(type); }

        //! Shortcut for retrieving a service. See ServiceManager::GetService() for more info
        template <class T>
        __inline const boost::weak_ptr<T> GetService(service_type_t type) const { return service_manager_->GetService<T>(type); }

        /// Returns the framework Input object.
        InputServiceInterface &Input();

        //! Creates new empty scene.
        /*! 
            \param name name of the new scene
            \return The new scene, or empty pointer if scene with the specified name already exists.
        */
        Scene::ScenePtr CreateScene(const std::string &name);

        //! Removes a scene with the specified name.
        /*! The scene may not get deleted since there may be dangling references to it.
            If the scene does get deleted, removes all entities which are not shared with
            another existing scene.

            Does nothing if scene with the specified name doesn't exist.

            \param name name of the scene to delete
        */
        void RemoveScene(const std::string &name);

        //! Returns a pointer to a scene
        /*! Manage the pointer carefully, as scenes may not get deleted properly if
            references to the pointer are left alive.

            \note Returns a shared pointer, but it is preferable to use a weak pointer, Scene::SceneWeakPtr,
                  to avoid dangling references that prevent scenes from being properly destroyed.

            \param name Name of the scene to return
            \return The scene, or empty pointer if the scene with the specified name could not be found
        */
        Scene::ScenePtr GetScene(const std::string &name) const;

        //! Returns true if specified scene exists, false otherwise
        bool HasScene(const std::string &name) const;

        //! Returns the currently set default world scene, for convinience
        const Scene::ScenePtr &GetDefaultWorldScene() const;

        //! Sets the default world scene, for convinient retrieval with GetDefaultWorldScene().
        void SetDefaultWorldScene(const Scene::ScenePtr &scene);

        //! Returns the scene map for self reflection / introspection.
        const SceneMap &GetSceneMap() const;

#ifdef PROFILING
        //! Returns the default profiler used by all normal profiling blocks. For profiling code, use PROFILE-macro.
        //! Profiler &GetProfiler() { return *ProfilerSection::GetProfiler(); }
        Profiler &GetProfiler();
#endif
        //! Add a new log listener for poco log
        void AddLogChannel(Poco::Channel *channel);

        //! Remove existing log listener from poco log
        void RemoveLogChannel(Poco::Channel *channel);

        //! load and init module
        Console::CommandResult ConsoleLoadModule(const StringVector &params);

        //! uninit and unload a module
        Console::CommandResult ConsoleUnloadModule(const StringVector &params);

        //! List all loaded modules
        Console::CommandResult ConsoleListModules(const StringVector &params);

        //! send event
        Console::CommandResult ConsoleSendEvent(const StringVector &params);

        //! Output profiling data
        Console::CommandResult ConsoleProfile(const StringVector &params);

        //! limit frames
        Console::CommandResult ConsoleLimitFrames(const StringVector &params);

        //! Returns name of the configuration group used by the framework
        /*! The group name is used with ConfigurationManager, for framework specific
            settings. Alternatively a class may use it's own name as the name of the
            configuration group, if it so chooses.

            For internal use only.
        */
        static const std::string &ConfigurationGroup()
        {
            static std::string group("Foundation");
            return group;
        }

        //! Loads all available modules. Do not call normally.
        void LoadModules();

        //! Unloads all available modules. Do not call normally.
        void UnloadModules();

        //! Get main QApplication
        QApplication *GetQApplication() const;

        //! Get main window
        MainWindow *GetMainWindow() const;

        //! Get main UI View
        QGraphicsView *GetUIView() const;

        //! Set main UI View
        void SetUIView(std::auto_ptr <QGraphicsView> view);

        /** Returns module by class T.
            @param T class type of the module.
            @return The module, or null if the module doesn't exist. Always remember to check for null pointer.
            @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it.
         */
        template <class T> T *GetModule()
        {
            return GetModuleManager()->GetModule<T>().lock().get();
        }

        /** Returns service by class T.
            @param T class type of the service.
            @return The service, or null if the service doesn't exist. Always remember to check for null pointer.
            @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it.
         */
        template <class T> T *GetService()
        {
            return GetServiceManager()->GetService<T>().lock().get();
        }

    signals:
        /** Emitted after one frame is processed.
         *  @param frametime Elapsed time in seconds since the last frame.
         */
        void FrameProcessed(double frametime);

    private:
        //! Registers framework specific console commands
        //! Should be called after modules are loaded and initialized
        void RegisterConsoleCommands();

        //! Create logging system
        void CreateLoggingSystem();

        //! Module manager.
        ModuleManagerPtr module_manager_;

        //! Component manager.
        ComponentManagerPtr component_manager_;

        //! Service manager.
        ServiceManagerPtr service_manager_;

        //! Event manager.
        EventManagerPtr event_manager_;

        //! Platform
        PlatformPtr platform_;

        //! Thread task manager.
        ThreadTaskManagerPtr thread_task_manager_;

        //! default configuration
        ConfigurationManagerPtr config_manager_;

        //! application data
        ApplicationPtr application_;

        //! if true, exit application
        bool exit_signal_;

        //! Logger channels
        std::vector<Poco::Channel*> log_channels_;

        //! Logger default formatter
        Poco::Formatter *log_formatter_;

        //! map of scenes
        SceneMap scenes_;

        //! Current 'default' scene
        Scene::ScenePtr default_scene_;

        //! Bridges QtApplication and Framework bridge object.
        std::auto_ptr <FrameworkQtApplication> engine_;

#ifdef PROFILING
        //! profiler
        Profiler profiler_;
#endif
        //! program options
        boost::program_options::variables_map cm_options_;

        //! program option descriptions
        boost::program_options::options_description cm_descriptions_;

        //! command line arguments as supplied by the operating system
        int argc_;
        char **argv_;

        //! Frame timer.
        boost::timer timer;

        //! true if framework is properly initialized, false otherwise.
        bool initialized_;

        //! Sends log prints for multiple channels.
        Poco::SplitterChannel *splitterchannel;
    };

    namespace
    {
        const event_id_t PROGRAM_OPTIONS = 1;
        const event_id_t NETWORKING_REGISTERED = 2;
        const event_id_t WORLD_STREAM_READY = 3;
    }

    //! Contains pre-parsed program options and non-parsed command line arguments.
    /*! Options contains program options pre-parsed by framework. If modules wish
        to use their own command line arguments, the arguments are also supplied.
    */
    class ProgramOptionsEvent : public EventDataInterface
    {
        ProgramOptionsEvent();
    public:
        ProgramOptionsEvent(const boost::program_options::variables_map &vars, int ac, char **av) :
            options(vars), argc(ac), argv(av) { }
        //! parsed program options
        const boost::program_options::variables_map &options;
        //! command line arguments as supplied by the operating system
        int argc;
        char **argv;
    };
}

#endif

