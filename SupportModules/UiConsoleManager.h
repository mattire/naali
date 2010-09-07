// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_SupportModules_UiConsoleManager_h
#define incl_SupportModules_UiConsoleManager_h

#include <QObject>
#include <QPropertyAnimation>

#include "CoreTypes.h"

namespace Foundation
{
    class Framework;
}

namespace Ui
{
    class ConsoleWidget;
}

class QString;
class QRectF;
class QGraphicsView;
class UiProxyWidget;

namespace Console
{
    class ConsoleProxyWidget;

    class UiConsoleManager: public QObject
    {
        Q_OBJECT

    public:
        UiConsoleManager(Foundation::Framework *framework, QGraphicsView *ui_view);
        virtual ~UiConsoleManager();

    public slots:
        //! Toggle console show/hide
        void ToggleConsole();

        //! Queues the print requests
        void QueuePrintRequest(const QString &text);

        //! Prints to console
        void PrintToConsole(const QString &text);

        //! Handle input to the console(called after return pushed)
        void HandleInput();

    signals:
        //! This emit is Qt::QueuedConnection type to avoid issues when printing from threads
        void PrintOrderReceived(const QString &text);

        //! TODO: comment
        void CommandIssued(const QString &string);

    private slots:
        //! Resize the console to fit scene
        void AdjustToSceneRect(const QRectF &);

    private:
        //! is console visible or hidden?
        bool visible_;

        //! if we have already hooked to all scenes for resize events
        bool hooked_to_scenes_;

        //! Method for coloring/styling the string for the console
        void StyleString(QString &str);

        //! Console opacity
        qreal opacity_;

        //! Console event category
        event_category_id_t console_category_id_;

        //! Framework pointer
        Foundation::Framework* framework_;

        //! View to the scene
        QGraphicsView *ui_view_;

        //! UI
        Ui::ConsoleWidget* console_ui_;

        //! Widget in UI
        QWidget * console_widget_;

        // Proxy for our UI
        UiProxyWidget *proxy_widget_;

        //! Animation used for sliding effect
        QPropertyAnimation animation_;
    };
}

#endif
