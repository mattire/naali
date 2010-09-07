// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "UiStateMachine.h"
#include "KeyBindings.h"

#include <QPropertyAnimation>
#include <QGraphicsWidget>
#include <QDebug>

#include "MemoryLeakCheck.h"

namespace CoreUi
{
    UiStateMachine::UiStateMachine(QGraphicsView *view, QObject *parent)
        : QStateMachine(parent),
          view_(view),
          current_scene_(view->scene()),
          current_scene_name_(""),
          connection_state_(UiServices::Disconnected)
    {
        state_ether_ = new QState(this);
        state_inworld_ = new QState(this);
        state_connecting_ = new QState(this);
        state_animating_change_ = new QState(this);

        SetTransitions();
        setInitialState(state_inworld_);
        start();
    }

    // Private

    void UiStateMachine::SetTransitions()
    {
        state_ether_->addTransition(this, SIGNAL( EtherTogglePressed()), state_inworld_);
        state_inworld_->addTransition(this, SIGNAL( EtherTogglePressed()), state_ether_);

        connect(state_ether_, SIGNAL( exited() ), SLOT( StateSwitch() ));
        connect(state_inworld_, SIGNAL( exited() ), SLOT( StateSwitch() ));
        connect(view_, SIGNAL( ViewKeyPressed(QKeyEvent *) ), SLOT( ViewKeyEvent(QKeyEvent *) ));
    }

    void UiStateMachine::ViewKeyEvent(QKeyEvent *key_event)
    {
        if (key_event->isAutoRepeat())
            return;      
        if (ether_toggle_seq_list_.contains(QKeySequence(key_event->key() + key_event->modifiers())))
            ToggleEther();
    }

    void UiStateMachine::UpdateKeyBindings(Foundation::KeyBindings *bindings)
    {
        std::list<Foundation::Binding> bind_list = bindings->GetBindings("naali.toggle.ether");
        std::list<Foundation::Binding>::const_iterator iter = bind_list.begin();
        std::list<Foundation::Binding>::const_iterator end = bind_list.end();

        if (iter == end)
            return;

        ether_toggle_seq_list_.clear();
        while (iter != end)
        {
            ether_toggle_seq_list_.append((*iter).sequence);
            iter++;
        }
    }

    void UiStateMachine::StateSwitch()
    {
        if (current_scene_ == scene_map_["Ether"])
            next_scene_name_ = "Inworld";
        else if (current_scene_ == scene_map_["Inworld"])
            next_scene_name_ = "Ether";
        else
        {
            if (connection_state_ == UiServices::Connected)
                next_scene_name_ = "Inworld";
            else
                next_scene_name_ = "Ether";
        }
        AnimationsStart();
    }

    void UiStateMachine::AnimationsStart()
    {
        // Create group if doesent exist yet
        QParallelAnimationGroup *animations_group;
        if (!animations_map_.contains(current_scene_))
        {
            animations_group = new QParallelAnimationGroup(this);
            animations_map_[current_scene_] = animations_group;
            connect(animations_group, SIGNAL( finished() ), SLOT( AnimationsFinished() ));
        }
        else
            animations_group = animations_map_[current_scene_];

        if (animations_group->state() == QAbstractAnimation::Running)
            return;

        animations_group->clear();
        foreach (QGraphicsItem *item, current_scene_->items())
        {
            QGraphicsWidget *widget = dynamic_cast<QGraphicsWidget*>(item);
            if (!widget)
                continue;
            if (!widget->isVisible())
                continue;

            QPropertyAnimation *anim = new QPropertyAnimation(widget, "opacity", animations_group);
            anim->setDuration(300);
            anim->setStartValue(widget->opacity());
            anim->setEndValue(0);
            animations_group->addAnimation(anim);
        }

        CheckAnimationTargets(animations_group);
        animations_group->setDirection(QAbstractAnimation::Forward);
        animations_group->start();
    }

    void UiStateMachine::AnimationsFinished()
    {
        if (animations_map_.contains(current_scene_))
            if (animations_map_[current_scene_]->direction() != QAbstractAnimation::Forward)
                return;

        QTimer::singleShot(100, this, SLOT(DelayedSceneChange()));
    }

    void UiStateMachine::DelayedSceneChange()
    {
        if (scene_map_.contains(next_scene_name_))
            SwitchToScene(next_scene_name_);
    }

    void UiStateMachine::CheckAnimationTargets(QParallelAnimationGroup *animations)
    {
        for (int i=0; i<animations->animationCount(); i++)
        {
            QPropertyAnimation *anim = dynamic_cast<QPropertyAnimation *>(animations->animationAt(i));
            if (!anim)
                continue;
            if (!anim->targetObject())
                animations->removeAnimation(anim);
        }
    }

    // Public

    void UiStateMachine::SwitchToInworldScene()
    {
        CheckAndSwitch("Inworld");
    }

    void UiStateMachine::SwitchToEtherScene()
    {
        CheckAndSwitch("Ether");
    }

    void UiStateMachine::SwitchToBuildScene()
    {
        CheckAndSwitch("WorldBuilding");
    }

    void UiStateMachine::CheckAndSwitch(const QString scene_name)
    {
        if (current_scene_)
        {
            QString current_name = scene_map_.key(current_scene_);
            if (current_name != scene_name)
                SwitchToScene(scene_name);
            else if (current_name.isEmpty() || current_name.isNull())
                SwitchToScene(scene_name);
        }
        else
            SwitchToScene(scene_name);
    }

    void UiStateMachine::ToggleEther()
    {
        if (connection_state_ == UiServices::Connected)
            emit EtherTogglePressed();
    }

    void UiStateMachine::RegisterScene(const QString &name, QGraphicsScene *scene)
    {
        if (!scene_map_.contains(name))
        {
            scene_map_[name] = scene;
            if (name == "Ether") // Ether notifies when its ready after connected
                connect(scene, SIGNAL(EtherSceneReadyForSwitch()), SLOT(SwitchToInworldScene()));
        }
    }

    bool UiStateMachine::UnregisterScene(const QString &name)
    {
        if (scene_map_.remove(name) > 0)
            return true;
        return false;
    }

    bool UiStateMachine::SwitchToScene(const QString &name)
    {
        if (!scene_map_.contains(name))
            return false;
        if (next_scene_name_ != name)
        {
            next_scene_name_ = name;
            AnimationsStart();
        }
        else
        {
            disconnect(current_scene_, SIGNAL( changed(const QList<QRectF> &) ), view_, SLOT( SceneChange() ));
         
            QString old_scene_name = current_scene_name_;
            current_scene_ = scene_map_[name];
            current_scene_name_ = name;

            current_scene_->setSceneRect(view_->viewport()->rect());
            if (view_->scene() != current_scene_)
                view_->setScene(current_scene_);

            connect(current_scene_, SIGNAL( changed(const QList<QRectF> &) ), view_, SLOT( SceneChange() ));

            if (animations_map_.contains(current_scene_))
            {
                QParallelAnimationGroup *animations = animations_map_[current_scene_];
                CheckAnimationTargets(animations);
                animations->setDirection(QAbstractAnimation::Backward);
                animations->start();
            }

            emit SceneChanged(old_scene_name, current_scene_name_);
            emit SceneChangeComplete();
        }
        return true;
    }

    QGraphicsScene *UiStateMachine::GetScene(const QString &name) const
    {
        if (scene_map_.contains(name))
            return scene_map_[name];
        else
            return 0;
    }

    void UiStateMachine::SetConnectionState(UiServices::ConnectionState new_connection_state)
    {
        connection_state_ = new_connection_state;

        switch (connection_state_)
        {
            case UiServices::Disconnected:
                SwitchToEtherScene();
                break;
            case UiServices::Connected:
                // Ether notifies when its animations are done after we are connected
                // and switch will happen then
                break;
            case UiServices::Failed:
                connection_state_ = UiServices::Disconnected;
                break;
            default:
                return;
        }
    }

    void UiStateMachine::SetServiceGetter(QObject *service_getter)
    {
        connect(service_getter, SIGNAL(KeyBindingsChanged(Foundation::KeyBindings*)),
                SLOT(UpdateKeyBindings(Foundation::KeyBindings*)));
    }

    void UiStateMachine::RegisterUniversalWidget(const QString &name, QGraphicsProxyWidget *widget)
    {
        if (universal_widgets_.contains(name))
            return;
        universal_widgets_[name] = widget;
    }
}
