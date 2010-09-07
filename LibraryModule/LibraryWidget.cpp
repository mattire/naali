// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "LibraryWidget.h"

#include <QGraphicsScene>
#include <QDebug>

#include "MemoryLeakCheck.h"


namespace Library
{

    LibraryWidget::LibraryWidget(QGraphicsView *ui_view) : 
            QWidget(),
                ui_view_(ui_view)
    {
        setupUi(this);        

        connect(urlPushButton, SIGNAL(clicked()), this, SLOT(SetWebViewUrl()));

        setMouseTracking(true);
        setAcceptDrops(true);
        webView->setMouseTracking(true);
        webView->setAcceptDrops(true);

    }

    void LibraryWidget::SetWebViewUrl()
    {          
        QString url = urlLineEdit->text();
        if (url.isEmpty())
        {
            return;
        }
        
        webView->setUrl(QUrl(url));        

    }

}
