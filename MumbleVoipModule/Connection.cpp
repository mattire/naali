// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "Connection.h"
#include "MumbleVoipModule.h"
#define BUILDING_DLL // for dll import/export declarations
#define CreateEvent CreateEventW // for \boost\asio\detail\win_event.hpp and \boost\asio\detail\win_iocp_handle_service.hpp
#include <mumbleclient/client.h>
#include <mumbleclient/client_lib.h>
#undef BUILDING_DLL // for dll import/export declarations
#include <mumbleclient/settings.h>
#include <mumbleclient/PacketDataStream.h>
#include <mumbleclient/channel.h>
#include <mumbleclient/user.h>
#include "SoundServiceInterface.h"
#include "Channel.h"
#include "User.h"
#include "PCMAudioFrame.h"
#include <QUrl>
#include <celt/celt_types.h>
#include <celt/celt.h>
#include <QMetaType>

#include "MemoryLeakCheck.h"

namespace MumbleLib
{
    // \todo Move these static callback functions to separeate file...

    void TextMessageCallback(const std::string& message, Connection* connection)
    {
        connection->HandleIncomingTextMessage(QString(message.c_str()));
    }

    void RawUdpTunnelCallback(int32_t length, void* buffer, Connection* connection)
    {
        connection->HandleIncomingRawUdpTunnelPacket(length, buffer);
    }

    void RelayTunnelCallback(int32_t length, void* buffer_, Connection* connection)
    {
     //   PacketDataStream data_stream = PacketDataStream((char*)buffer_, length);
     //   bool valid = data_stream.isValid();
     //   int size = data_stream.size();
     //   char* buffer = (char*)buffer_;
     //   short type = buffer[0] << 8 + buffer[1];
     //   int len = buffer[3] << 16 + buffer[4] << 8 + buffer[5];
	    //std::string s(static_cast<char *>(buffer), length);
	    //s.erase(1, pds_int_len(&static_cast<char *>(buffer)[1]));
    //    connection->OnRelayTunnel(s);
    }

    void AuthCallback(Connection* connection)
    {
        connection->SetAuthenticated();
    }

    void ChannelAddCallback(const MumbleClient::Channel& channel, Connection* connection)
    {
        connection->AddChannel(channel);
    }

    void ChannelRemoveCallback(const MumbleClient::Channel& channel, Connection* connection)
    {
        connection->RemoveChannel(channel);
    }

    void UserJoinedCallback(const MumbleClient::User& user, Connection* connection)
    {
        connection->CreateUserObject(user);
    }

    void UserLeftCallback(const MumbleClient::User& user, Connection* connection)
    {
        connection->MarkUserLeft(user);
    }

    Connection::Connection(MumbleVoip::ServerInfo &info, int playback_buffer_length_ms) :
            client_(0),
            authenticated_(false),
            celt_mode_(0),
            celt_encoder_(0),
            celt_decoder_(0),
            sending_audio_(false),
            receiving_audio_(true),
            frame_sequence_(0),
            encoding_quality_(0),
            state_(STATE_CONNECTING),
            send_position_(false),
            playback_buffer_length_ms_(playback_buffer_length_ms),
            statistics_(500)
    {
        // BlockingQueuedConnection for cross thread signaling
        QObject::connect(this, SIGNAL(UserObjectCreated(User*)), SLOT(AddToUserList(User*)), Qt::BlockingQueuedConnection);

        InitializeCELT();

        MumbleClient::MumbleClientLib* mumble_lib = MumbleClient::MumbleClientLib::instance();
        QMutexLocker client_locker(&mutex_client_);
        client_ = mumble_lib->NewClient();

        QUrl server_url(QString("mumble://%1").arg(info.server));

        QString port = QString::number(server_url.port(MUMBLE_DEFAULT_PORT_)); // default port name
        QString server = server_url.host();

        // \todo Handle connection error
	    client_->SetRawUdpTunnelCallback( boost::bind(&RawUdpTunnelCallback, _1, _2, this));
        client_->SetChannelAddCallback(boost::bind(&ChannelAddCallback, _1, this));
        client_->SetChannelRemoveCallback(boost::bind(&ChannelRemoveCallback, _1, this));
        client_->SetTextMessageCallback(boost::bind(&TextMessageCallback, _1, this));
        client_->SetAuthCallback(boost::bind(&AuthCallback, this));
        client_->SetUserJoinedCallback(boost::bind(&UserJoinedCallback, _1, this));
        client_->SetUserLeftCallback(boost::bind(&UserLeftCallback, _1, this));
        try
        {
            client_->Connect(MumbleClient::Settings(server.toStdString(), port.toStdString(), info.user_name.toStdString(), info.password.toStdString()));
        }
        catch(std::exception &e)
        {
            lock_state_.lockForWrite();
            state_ = STATE_ERROR;
            lock_state_.unlock();
            reason_ = QString(e.what());
            return;
        }
        lock_state_.lockForWrite();
        state_ = STATE_AUTHENTICATING;
        lock_state_.unlock();
        emit StateChanged(state_);

        connect(&user_update_timer_, SIGNAL(timeout()), SLOT(UpdateUserStates()));
        user_update_timer_.start(USER_STATE_CHECK_TIME_MS);
    }

    Connection::~Connection()
    {
        Close();
        this->disconnect();

        QMutexLocker locker1(&mutex_raw_udp_tunnel_);

        UninitializeCELT();

        QMutexLocker locker2(&mutex_encode_queue_);
        while (encode_queue_.size() > 0)
        {
            MumbleVoip::PCMAudioFrame* frame = encode_queue_.takeFirst();
            SAFE_DELETE(frame);
        }

        QMutexLocker locker3(&mutex_channels_);
        while (channels_.size() > 0)
        {
            Channel* c = channels_.takeFirst();
            SAFE_DELETE(c);
        }

        lock_users_.lockForWrite();
        foreach(User* u, users_)
        {
            SAFE_DELETE(u);
        }
        lock_users_.unlock();

        lock_state_.lockForRead();
        if (state_ != STATE_ERROR)
        {
            QMutexLocker client_locker(&mutex_client_);
            SAFE_DELETE(client_);
        }
        lock_state_.unlock();
    }

    Connection::State Connection::GetState() const
    {
        //lock_state_.lockForRead(); // cannot be call because const 
        Connection::State state = state_;
        //lock_state_.unlock();
        return state;
    }

    QString Connection::GetReason() const
    { 
        return reason_;
    }

    void Connection::Close()
    {
        user_update_timer_.stop();
        QMutexLocker raw_udp_tunnel_locker(&mutex_raw_udp_tunnel_);
        lock_state_.lockForWrite();
        QMutexLocker client_locker(&mutex_client_);
        if (state_ != STATE_CLOSED && state_ != STATE_ERROR)
        {
            try
            {
                client_->Disconnect();
            }
            catch(std::exception &e)
            {
                state_ = STATE_ERROR;
                reason_ = QString(e.what());
            }
            state_ = STATE_CLOSED;
            lock_state_.unlock();
            emit StateChanged(state_);
        }
        else
            lock_state_.unlock();
    }

    void Connection::InitializeCELT()
    {
        int error = 0;
        celt_mode_ = celt_mode_create(MumbleVoip::SAMPLE_RATE, MumbleVoip::SAMPLES_IN_FRAME, &error );
        if (error != 0)
        {
            QString message = QString("CELT initialization failed, error code = %1").arg(error);
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            lock_state_.lockForWrite();
            state_ = STATE_ERROR;
            lock_state_.unlock();
            emit StateChanged(state_);
            return;
        }

        QMutexLocker encoder_locker(&mutex_encoder_);
        celt_encoder_ = celt_encoder_create(celt_mode_,MumbleVoip::NUMBER_OF_CHANNELS, NULL );
        if (!celt_encoder_)
        {
            QString message = QString("Cannot create CELT encoder");
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            lock_state_.lockForWrite();
            state_ = STATE_ERROR;
            lock_state_.unlock();
            emit StateChanged(state_);
            return;
        }
        celt_encoder_ctl(celt_encoder_, CELT_SET_PREDICTION(0));
	    celt_encoder_ctl(celt_encoder_, CELT_SET_VBR_RATE(BitrateForDecoder()));

        celt_decoder_ = CreateCELTDecoder();

        MumbleVoip::MumbleVoipModule::LogDebug("CELT initialized.");
    }

    void Connection::UninitializeCELT()
    {
        QMutexLocker encoder_locker(&mutex_encoder_);
        celt_encoder_destroy(celt_encoder_);
        celt_encoder_ = 0;
        celt_decoder_destroy(celt_decoder_);
        celt_decoder_ = 0;
        celt_mode_destroy(celt_mode_);
        celt_mode_ = 0;
        MumbleVoip::MumbleVoipModule::LogDebug("CELT uninitialized.");
    }

    CELTDecoder* Connection::CreateCELTDecoder()
    {
        int error = 0;
        CELTDecoder* decoder = celt_decoder_create(celt_mode_,MumbleVoip::NUMBER_OF_CHANNELS, &error);
        switch (error)
        {
        case CELT_OK:
           return decoder;
        case CELT_BAD_ARG:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: CELT_BAD_ARG");
            return 0;
        case CELT_INVALID_MODE:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: CELT_INVALID_MODE");
            return 0;
        case CELT_INTERNAL_ERROR:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: CELT_INTERNAL_ERROR");
            return 0;
        case CELT_UNIMPLEMENTED:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: CELT_UNIMPLEMENTED");
            return 0;
        case CELT_ALLOC_FAIL:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: CELT_ALLOC_FAIL");
            return 0;
        default:
            MumbleVoip::MumbleVoipModule::LogError("Cannot create CELT decoder: unknow reason");
            return 0;
        }
    }

    void Connection::Join(QString channel_name)
    {
        QMutexLocker locker1(&mutex_authentication_);
        QMutexLocker locker2(&mutex_channels_);

        if (!authenticated_)
        {
            join_request_ = channel_name;
            return; 
        }

        foreach(Channel* channel, channels_)
        {
            if (channel->FullName() == channel_name)
            {
                Join(channel);
            }
        }
    }

    void Connection::Join(const Channel* channel)
    {
        QMutexLocker client_locker(&mutex_client_);
        client_->JoinChannel(channel->Id());
    }

    AudioPacket Connection::GetAudioPacket()
    {
        lock_users_.lockForRead();
        if (users_.size() == 0)
        {
            lock_users_.unlock();
            return AudioPacket(0,0);
        }

        QList<User*> user_list = users_.values();

        static int first_index = 0; // We want to give fair selection method for all user objects 
        first_index = (first_index + 1) % user_list.size();

        for (int i = 0; i < user_list.size(); ++i)
        {
            int index = (first_index + i) % user_list.size();
            User* user = user_list[index];

            if (!user->tryLock())
                continue;

            MumbleVoip::PCMAudioFrame* frame = user->GetAudioFrame();
            user->unlock();
            if (frame)
            {
                first_index = i; // we want start next round from one user after this one
                lock_users_.unlock();
                return AudioPacket(user, frame);
            }
        }
        lock_users_.unlock();
        return AudioPacket(0,0);
    }

    void Connection::SendAudio(bool send)
    {
        sending_audio_ = send;
    }

    bool Connection::SendingAudio() const
    {
        return sending_audio_;
    }

    void Connection::ReceiveAudio(bool receive)
    {
        receiving_audio_ = receive;
    }

    void Connection::SendAudioFrame(MumbleVoip::PCMAudioFrame* frame, Vector3df users_position)
    {
        QMutexLocker locker(&mutex_encode_queue_);

        lock_state_.lockForRead();
        if (state_ != STATE_OPEN)
        {
            lock_state_.unlock();
            return;
        }
        lock_state_.unlock();

        MumbleVoip::PCMAudioFrame* f = new MumbleVoip::PCMAudioFrame(frame);
        encode_queue_.push_back(f);
        
        if (encode_queue_.size() < MumbleVoip::FRAMES_PER_PACKET)
            return;

        QMutexLocker encoder_locker(&mutex_encoder_);

        for (int i = 0; i < MumbleVoip::FRAMES_PER_PACKET; ++i)
        {
            MumbleVoip::PCMAudioFrame* audio_frame = encode_queue_.takeFirst();

            int32_t len = celt_encode(celt_encoder_, reinterpret_cast<short *>(audio_frame->DataPtr()), NULL, encode_buffer_, std::min(BitrateForDecoder() / (100 * 8), 127));
            memcpy(encoded_frame_data_[i], encode_buffer_, len);
            encoded_frame_length_[i] = len;
            assert(len < ENCODE_BUFFER_SIZE_);

            delete audio_frame;
        }
        const int PACKET_DATA_SIZE_MAX = 1024;
	    static char data[PACKET_DATA_SIZE_MAX];
	    int flags = 0; // target = 0
	    flags |= (MumbleClient::UdpMessageType::UDPVoiceCELTAlpha << 5);
	    data[0] = static_cast<unsigned char>(flags);
        PacketDataStream data_stream(data + 1, PACKET_DATA_SIZE_MAX - 1);
        data_stream << frame_sequence_;

        for (int i = 0; i < MumbleVoip::FRAMES_PER_PACKET; ++i)
        {
		    unsigned char head = encoded_frame_length_[i];
		    // Add 0x80 to all but the last frame
            if (i < MumbleVoip::FRAMES_PER_PACKET - 1)
			    head |= 0x80;

		    data_stream.append(head);
            data_stream.append(encoded_frame_data_[i], encoded_frame_length_[i]);

            frame_sequence_++;
	    }
        if (send_position_)
        {
            // Coordinate conversion: Naali -> Mumble
            data_stream << static_cast<float>(users_position.y);
            data_stream << static_cast<float>(users_position.z);
            data_stream << static_cast<float>(-users_position.x);
        }
        mutex_client_.lock();
        statistics_.NotifyBytesSent(data_stream.size() + 1);
        client_->SendRawUdpTunnel(data, data_stream.size() + 1 );
        mutex_client_.unlock();
    }

    void Connection::SetAuthenticated()
    {
        lock_state_.lockForRead();
        if (state_ != STATE_AUTHENTICATING)
        {
            lock_state_.unlock();
            QString message = QString("Authentication notification received but state = %1").arg(state_);
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            return;
        }
        lock_state_.unlock();

        mutex_authentication_.lock();
        authenticated_ = true;
        mutex_authentication_.unlock();

        if (join_request_.length() > 0)
        {
            QString channel = join_request_;
            join_request_ = "";
            Join(channel);
        }

        lock_state_.lockForWrite();
        state_ = STATE_OPEN;
        lock_state_.unlock();
        emit StateChanged(state_);
    }

    void Connection::HandleIncomingTextMessage(QString text)
    {
        emit (TextMessageReceived(text));
    }

    void Connection::AddChannel(const MumbleClient::Channel& new_channel)
    {
        QMutexLocker locker(&mutex_channels_);

        foreach(Channel* c, channels_)
        {
            if (c->Id() == new_channel.id)
                return;
        }

        Channel* c = new Channel(&new_channel);
        channels_.append(c);
        QString message = QString("Channel '%1' added").arg(c->Name());
        MumbleVoip::MumbleVoipModule::LogDebug(message.toStdString());
    }

    void Connection::RemoveChannel(const MumbleClient::Channel& channel)
    {
        QMutexLocker locker(&mutex_channels_);

        int i = 0;
        for (int i = 0; i < channels_.size(); ++i)
        {
            if (channels_.at(i)->Id() == channel.id)
            {
                Channel* c = channels_.at(i);
                channels_.removeAt(i);
                QString message = QString("Channel '%1' removed").arg(c->Name());
                MumbleVoip::MumbleVoipModule::LogDebug(message.toStdString());
                SAFE_DELETE(c);
                break;
            }
        }
    }

    void Connection::HandleIncomingRawUdpTunnelPacket(int length, void* buffer)
    {
        statistics_.NotifyBytesReceived(length);

        if (!receiving_audio_)
            return;
        
        if (!mutex_raw_udp_tunnel_.tryLock())
            return;

        mutex_raw_udp_tunnel_.unlock();
		QMutexLocker raw_udp_tunnel_locker(&mutex_raw_udp_tunnel_);

        if (lock_state_.tryLockForRead(10))
        {
            if (state_ != STATE_OPEN)
            {
                lock_state_.unlock();
                return;
            }
            lock_state_.unlock();
        }
        else
            return;

        PacketDataStream data_stream = PacketDataStream((char*)buffer, length);
        bool valid = data_stream.isValid();

        uint8_t first_byte = static_cast<unsigned char>(data_stream.next());
        MumbleClient::UdpMessageType::MessageType type = static_cast<MumbleClient::UdpMessageType::MessageType>( ( first_byte >> 5) & 0x07 );
        uint8_t flags = first_byte & 0x1f;
        switch (type)
        {
        case MumbleClient::UdpMessageType::UDPVoiceCELTAlpha:
            break;
        case MumbleClient::UdpMessageType::UDPPing:
            MumbleVoip::MumbleVoipModule::LogDebug("Unsupported packet received: MUMBLE-UDP PING");
            return;
            break;
        case MumbleClient::UdpMessageType::UDPVoiceSpeex:
            MumbleVoip::MumbleVoipModule::LogDebug("Unsupported packet received: MUMBLE-UDP Speex audio frame");
            return;
            break;
        case MumbleClient::UdpMessageType::UDPVoiceCELTBeta:
            MumbleVoip::MumbleVoipModule::LogDebug("Unsupported packet received: MUMBLE-UDP CELT B audio frame");
            return;
            break;
        }

        int session;
        int seq;
        data_stream >> session;
        data_stream >> seq;

        bool last_frame = true;
        do
        {
		    int header = static_cast<unsigned char>(data_stream.next());
            int frame_size = header & 0x7f;
            last_frame = !(header & 0x80);
            const char* frame_data = data_stream.charPtr();
            data_stream.skip(frame_size);

            if (frame_size > 0)
				HandleIncomingCELTFrame(session, (unsigned char*)frame_data, frame_size);
	    }
        while (!last_frame && data_stream.isValid());
        if (!data_stream.isValid())
        {
            MumbleVoip::MumbleVoipModule::LogWarning("Syntax error in RawUdpTunnel packet.");
        }

        int bytes_left = data_stream.left();
        if (bytes_left)
        {
            // Coordinate conversion: Mumble -> Naali 
            Vector3df position;

            data_stream >> position.y;
            data_stream >> position.z;
            data_stream >> position.x;
            position.x *= -1;

            // position update is not required for every audio packet so we can skip
            // this if users_ is locked
            if (lock_users_.tryLockForRead())
            {
                User* user = users_[session];
                if (user)
                {
                    if (user->tryLock())
                    {
                        user->UpdatePosition(position);
                        user->unlock();
                    }
                }
                lock_users_.unlock();
            }
        }
    }

    void Connection::CreateUserObject(const MumbleClient::User& mumble_user)
    {
        AddChannel(*mumble_user.channel.lock().get());
        Channel* channel = ChannelById(mumble_user.channel.lock()->id);
        if (!channel)
        {
            QString message = QString("Cannot create user '%1': Channel doesn't exist.").arg(QString(mumble_user.name.c_str()));
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            return;
        }
        User* user = new User(mumble_user, channel);
        user->SetPlaybackBufferMaxLengthMs(playback_buffer_length_ms_);
        user->moveToThread(this->thread()); //! @todo Do we need this?
        
        emit UserObjectCreated(user);
    }

    void Connection::AddToUserList(User* user)
    {
        lock_users_.lockForWrite();
        users_[user->Session()] = user;
        lock_users_.unlock();

        QString message = QString("User '%1' joined.").arg(user->Name());
        MumbleVoip::MumbleVoipModule::LogDebug(message.toStdString());
        emit UserJoinedToServer(user);
    }

    void Connection::MarkUserLeft(const MumbleClient::User& mumble_user)
    {
        lock_users_.lockForWrite();

        if (!users_.contains(mumble_user.session))
        {
            QString message = QString("Unknow user '%1' Left.").arg(QString(mumble_user.name.c_str()));
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            lock_users_.unlock();
            return;
        }

        User* user = users_[mumble_user.session];
        QMutexLocker user_locker(user);

        QString message = QString("User '%1' Left.").arg(user->Name());
        MumbleVoip::MumbleVoipModule::LogDebug(message.toStdString());
        user->SetLeft();
        lock_users_.unlock();
        emit UserLeftFromServer(user);
    }

    QList<Channel*> Connection::ChannelList() 
    {
        QMutexLocker locker(&mutex_channels_);

        QList<Channel*> channels;
        foreach(Channel* c, channels_)
        {
            channels.append(c);
        }
        return channels;
    }

    MumbleLib::Channel* Connection::ChannelById(int id) 
    {
        QMutexLocker locker(&mutex_channels_);

        foreach(Channel* c, channels_)
        {
            if (c->Id() == id)
                return c;
        }
        return 0;
    }

    MumbleLib::Channel* Connection::ChannelByName(QString name) 
    {
        QMutexLocker locker(&mutex_channels_);

        foreach(Channel* c, channels_)
        {
            if (c->FullName() == name)
                return c;
        }
        return 0;
    }

    void Connection::HandleIncomingCELTFrame(int session, unsigned char* data, int size)
    {
        lock_users_.lockForRead();
        User* user = users_[session];
        lock_users_.unlock();

        if (!user)
        {
            QString message = QString("Audio frame from unknown user: %1").arg(session);
            MumbleVoip::MumbleVoipModule::LogWarning(message.toStdString());
            return;
        }

        MumbleVoip::PCMAudioFrame* audio_frame = new MumbleVoip::PCMAudioFrame(MumbleVoip::SAMPLE_RATE, MumbleVoip::SAMPLE_WIDTH, MumbleVoip::NUMBER_OF_CHANNELS, MumbleVoip::SAMPLES_IN_FRAME*MumbleVoip::SAMPLE_WIDTH/8);
        int ret = celt_decode(celt_decoder_, data, size, (short*)audio_frame->DataPtr());

        switch (ret)
        {
        case CELT_OK:
            {
                if (user->tryLock(5)) // 5 ms
                {
                    user->AddToPlaybackBuffer(audio_frame);
                    user->unlock();
                    return;
                }
                else
                {
                    MumbleVoip::MumbleVoipModule::LogWarning("Audio packet dropped: user object locked");
                }
            }
            break;
        case CELT_BAD_ARG:
            MumbleVoip::MumbleVoipModule::LogError("CELT decoding error: CELT_BAD_ARG");
            break;
        case CELT_INVALID_MODE:
            MumbleVoip::MumbleVoipModule::LogError("CELT decoding error: CELT_INVALID_MODE");
            break;
        case CELT_INTERNAL_ERROR:
            MumbleVoip::MumbleVoipModule::LogError("CELT decoding error: CELT_INTERNAL_ERROR");
            break;
        case CELT_CORRUPTED_DATA:
            MumbleVoip::MumbleVoipModule::LogError("CELT decoding error: CELT_CORRUPTED_DATA");
            break;
        case CELT_UNIMPLEMENTED:
            MumbleVoip::MumbleVoipModule::LogError("CELT decoding error: CELT_UNIMPLEMENTED");
            break;
        }
        delete audio_frame;
    }

    void Connection::SetEncodingQuality(double quality)
    {
        mutex_encoding_quality_.lock();
        if (quality < 0)
            quality = 0;
        if (quality > 1.0)
            quality = 1.0;
        encoding_quality_ = quality;
        mutex_encoding_quality_.unlock();

        QMutexLocker encoder_locker(&mutex_encoder_);
        celt_encoder_ctl(celt_encoder_, CELT_SET_VBR_RATE(BitrateForDecoder()));
    }
    
    int Connection::BitrateForDecoder()
    {
        QMutexLocker locker(&mutex_encoding_quality_);
        return static_cast<int>(encoding_quality_*(AUDIO_BITRATE_MAX_ - AUDIO_BITRATE_MIN_) + AUDIO_BITRATE_MIN_);
    }

    void Connection::UpdateUserStates()
    {
        lock_users_.lockForRead();
        foreach(User* user, users_)
        {
            QMutexLocker user_locker(user);
            user->CheckSpeakingState();
            Channel* channel = ChannelById(user->CurrentChannelID());
            user->SetChannel(channel);
        }
        lock_users_.unlock();
    }

    void Connection::SetPlaybackBufferMaxLengthMs(int length)
    {
        playback_buffer_length_ms_ = length;
        lock_users_.lockForRead();
        foreach(User* user, users_)
        {
            QMutexLocker user_locker(user);
            user->SetPlaybackBufferMaxLengthMs(length);
        }
        lock_users_.unlock();
    }

    int Connection::GetAverageBandwithIn() const
    {
        return statistics_.GetAverageBandwidthIn();
    }

    int Connection::GetAverageBandwithOut() const
    {
        return statistics_.GetAverageBandwidthOut();
    }

} // namespace MumbleLib 
