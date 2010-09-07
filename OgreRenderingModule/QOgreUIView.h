// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_OgreRenderer_QOgreUIView_h
#define incl_OgreRenderer_QOgreUIView_h

//#include "StableHeaders.h"

#include "QOgreWorldView.h"

#include <OgreRenderWindow.h>

#include <QGraphicsView>
#include <QKeyEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>

namespace Foundation { class KeyBindings; }

namespace OgreRenderer
{
    class QOgreUIView : public QGraphicsView
    {

    Q_OBJECT
    Q_PROPERTY(bool dirty_ WRITE setDirty READ isDirty)

    public:
        QOgreUIView (QWidget *parent);
        virtual ~QOgreUIView ();

        void SetWorldView(QOgreWorldView *view);
        void SetScene(QGraphicsScene *new_scene);
        void InitializeWorldView(int width, int height);
        
        Ogre::RenderWindow *CreateRenderWindow (const std::string &name, int width, int height, int left, int top, bool fullscreen);

    public slots:
        void setDirty(bool dirty) { dirty_ = dirty; }
        bool isDirty() { return dirty_; }

        void UpdateKeyBindings(Foundation::KeyBindings *bindings);

    protected:
        void keyPressEvent (QKeyEvent *e);
        void resizeEvent (QResizeEvent *e);
        void dropEvent (QDropEvent * event); 
        void dragEnterEvent (QDragEnterEvent * event);
        void dragMoveEvent (QDragMoveEvent * event);

    private:
        void Initialize_ ();

        Ogre::RenderWindow  *win_;
        QOgreWorldView *view_;
        bool dirty_;

        QList<QKeySequence> python_run_keys_;
        QList<QKeySequence> console_toggle_keys_;

    private slots:
        void SceneChange();

    signals:
        void ConsoleToggleRequest();
        void PythonRestartRequest();
        
        void ViewKeyPressed(QKeyEvent *key_event);

        void LibraryDropEvent(QDropEvent *drop_event);
    };

} 

#endif // incl_OgreRenderer_QOgreUIView_h
