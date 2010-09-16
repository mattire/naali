// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "VoiceControl.h"

namespace CommUI
{
    VoiceStateWidget::VoiceStateWidget(QWidget * parent, Qt::WindowFlags f)
        : QPushButton(parent),
          state_(STATE_OFFLINE)
    {
        setMinimumSize(42,32);
        setObjectName("stateIndicatorWidget"); // There can be obly one instance of this class
        UpdateStyleSheet();
        update_timer_.start(VOICE_ACTIVITY_UPDATE_INTERVAL_MS_);
        update_timer_.setSingleShot(false);
        connect(&update_timer_, SIGNAL(timeout()), SLOT(UpdateVoiceActivity()) );
    }

    void VoiceStateWidget::setState(State state)
    {
        State old_state = state_;
        state_ = state;
        if (old_state == STATE_OFFLINE && state == STATE_ONLINE)
            voice_activity_ = 1.0; // for notificate user about voice tranmission set on
        UpdateStyleSheet();
        emit StateChanged();
    }

    VoiceStateWidget::State VoiceStateWidget::state() const
    {
        return state_;
    }

    void VoiceStateWidget::UpdateVoiceActivity()
    {
        voice_activity_ -= static_cast<double>(VOICE_ACTIVITY_UPDATE_INTERVAL_MS_)/VOICE_ACTIVITY_FADEOUT_MAX_MS_;
        if (voice_activity_ < 0)
            voice_activity_ = 0;
        UpdateStyleSheet();
    }

    void VoiceStateWidget::SetVoiceActivity(double activity)
    {
        if (activity > 1)
            activity = 1;
        if (activity < 0)
            activity = 0;
        if (activity > voice_activity_)
            voice_activity_ = activity;

        UpdateStyleSheet();
    }

    void VoiceStateWidget::UpdateStyleSheet()
    {
        if (state_ == STATE_OFFLINE)
        {
            setStyleSheet("QPushButton#stateIndicatorWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/status_offline.png'); background-position: top left; background-repeat: no-repeat; }");
            voice_activity_ = 0;
            return;
        }

        if (voice_activity_ > 0.60)
        {
            setStyleSheet("QPushButton#stateIndicatorWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/voice_5.png'); background-position: top left; background-repeat: no-repeat; }");
            return;
        }

        if (voice_activity_ > 0.30)
        {
            setStyleSheet("QPushButton#stateIndicatorWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/voice_3.png'); background-position: top left; background-repeat: no-repeat; }");
            return;
        }

        if (voice_activity_ > 0.5)
        {
            setStyleSheet("QPushButton#stateIndicatorWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/voice_1.png'); background-position: top left; background-repeat: no-repeat; }");
            return;
        }
        setStyleSheet("QPushButton#stateIndicatorWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/status_online.png'); background-position: top left; background-repeat: no-repeat; }");
    }

    VoiceUsersInfoWidget::VoiceUsersInfoWidget(QWidget* parent)
        : QPushButton(parent),
          count_label_(this),
          user_count_(0)
    {
        count_label_.setObjectName("voiceUserCount");
        setMinimumSize(64,32);
        setObjectName("voiceUsersInfoWidget"); // There can be obly one instance of this class
        UpdateStyleSheet();
    }

    void VoiceUsersInfoWidget::SetUsersCount(int count)
    {
        user_count_ = count;
        UpdateStyleSheet();
    }

    int VoiceUsersInfoWidget::UsersCount() const
    {
        return user_count_;
    }

    void VoiceUsersInfoWidget::SetVoiceActivity(double activity)
    {
        //! \todo: IMPELEMENT
    }

    void VoiceUsersInfoWidget::UpdateStyleSheet()
    {
       setStyleSheet("QPushButton#voiceUsersInfoWidget { border: 0px; background-color: rgba(34,34,34,191); background-image: url('./data/ui/images/comm/user.png'); background-position: top left; background-repeat: no-repeat; }");
       count_label_.setStyleSheet("QLabel#voiceUserCount { border: 0px; background-color: transparent; background-position: top left; background-repeat: no-repeat; color: rgb(255,255,255); }");

       if (user_count_ == 0)
           count_label_.setText("0");
       else
           count_label_.setText(QString::number(user_count_));
    }



} // CommUI
