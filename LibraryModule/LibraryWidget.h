// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Library_LibraryWidget_h
#define incl_Library_LibraryWidget_h

#include "NetworkEvents.h"
#include "UiModuleApi.h"
#include "ui_LibraryWidget.h"

#include <QGraphicsProxyWidget>

namespace Library
{
    class LibraryWidget : public QWidget, private Ui::LibraryWidget
    {
        
    Q_OBJECT

    public:        
        LibraryWidget(QGraphicsView *ui_view);

    private:
        QGraphicsView * ui_view_;

    private slots:
        void SetWebViewUrl();

    };
}

#endif // incl_Library_LibraryWidget_h
