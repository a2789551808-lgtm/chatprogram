#include "tcpmgr.h"
#include <QJsonDocument>
TcpMgr::~TcpMgr()
{

}

TcpMgr::TcpMgr()
    : _host(""), _port(0), _b_recv_pending(false), _message_id(0), _message_len(0)
{
    QObject::connect(&_socket, &QTcpSocket::connected, [this]() {
        qDebug() << "Connected to server!";
        emit sig_con_success(true);
    });

    QObject::connect(&_socket, &QTcpSocket::readyRead, [this]() {
        _buffer.append(_socket.readAll());

        static const int kHeaderSize = static_cast<int>(sizeof(quint16) * 2);
        static const quint16 kMaxBodyLen = 60 * 1024; // 按需调整上限

        forever {
            if (!_b_recv_pending) {
                if (_buffer.size() < kHeaderSize) {
                    return; // 头不完整
                }

                QDataStream stream(&_buffer, QIODevice::ReadOnly);
                stream.setVersion(QDataStream::Qt_5_0);
                stream.setByteOrder(QDataStream::BigEndian); // 显式大端
                stream >> _message_id >> _message_len;

                if (stream.status() != QDataStream::Ok) {
                    qWarning() << "parse header failed";
                    return;
                }

                if (_message_len > kMaxBodyLen) {
                    qWarning() << "invalid message length:" << _message_len;
                    _buffer.clear();
                    _b_recv_pending = false;
                    return;
                }

                _buffer.remove(0, kHeaderSize); // 移除头部
                qDebug() << "Message ID:" << _message_id << ", Length:" << _message_len;
            }

            if (_buffer.size() < _message_len) {
                _b_recv_pending = true; // 半包
                return;
            }

            _b_recv_pending = false;

            QByteArray messageBody = _buffer.left(_message_len);
            _buffer.remove(0, _message_len);

            qDebug() << "receive body msg is" << messageBody;
            handleMsg(ReqId(_message_id), _message_len, messageBody);
        }
    });

    //5.15 之后版本
          QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), [&](QAbstractSocket::SocketError socketError) {
              Q_UNUSED(socketError)
              qDebug() << "Error:" << _socket.errorString();
          });
/*
    处理错误（适用于Qt 5.15之前的版本）
    QObject::connect(&_socket, static_cast<void (QTcpSocket::*)(QTcpSocket::SocketError)>(&QTcpSocket::error),
                     [&](QTcpSocket::SocketError socketError) {
                         qDebug() << "Error:" << _socket.errorString() ;
                         switch (socketError) {
                         case QTcpSocket::ConnectionRefusedError:
                             qDebug() << "Connection Refused!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::RemoteHostClosedError:
                             qDebug() << "Remote Host Closed Connection!";
                             break;
                         case QTcpSocket::HostNotFoundError:
                             qDebug() << "Host Not Found!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::SocketTimeoutError:
                             qDebug() << "Connection Timeout!";
                             emit sig_con_success(false);
                             break;
                         case QTcpSocket::NetworkError:
                             qDebug() << "Network Error!";
                             break;
                         default:
                             qDebug() << "Other Error!";
                             break;
                         }
                     });
*/

    // 处理连接断开
    QObject::connect(&_socket, &QTcpSocket::disconnected, [&]() {
        qDebug() << "Disconnected from server.";
    });
    //连接发送信号来发送数据
    QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
    //注册消息
    initHandlers();
}

void TcpMgr::initHandlers()
{
    //auto self = shared_from_this();
    _handlers.insert(ID_CHAT_LOGIN_RSP, [this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        qDebug()<< "handle id is "<< id ;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        qDebug()<< "data jsonobj is " << jsonObj ;

        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "Login Failed, err is Json Parse Err" << err ;
            emit sig_login_failed(err);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "Login Failed, err is " << err ;
            emit sig_login_failed(err);
            return;
        }

        auto uid = jsonObj["uid"].toInt();
        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto desc = jsonObj["desc"].toString();
        // auto user_info = std::make_shared<UserInfo>(uid, name, nick, icon, sex,"",desc);

        // UserMgr::GetInstance()->SetUserInfo(user_info);
        // UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());
        // if(jsonObj.contains("apply_list")){
        //     UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        // }

        //添加好友列表
        // if (jsonObj.contains("friend_list")) {
        //     UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        // }

        emit sig_swich_chatdlg();
    });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto find_iter =  _handlers.find(id);
    if(find_iter == _handlers.end()){
        qDebug()<< "not found id ["<< id << "] to handle";
        return ;
    }

    find_iter.value()(id,len,data);
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug()<< "receive tcp connect signal";
    // 尝试连接到服务器
    qDebug() << "Connecting to server...";
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _socket.connectToHost(si.Host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes)
{
    uint16_t id = reqId;

    // 计算长度（使用网络字节序转换）
    quint16 len = static_cast<quint16>(dataBytes.length());

    // 创建一个QByteArray用于存储要发送的所有数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 设置数据流使用网络字节序
    out.setByteOrder(QDataStream::BigEndian);

    // 写入ID和长度
    out << id << len;

    // 添加字符串数据
    block.append(dataBytes);

    // 发送数据
    _socket.write(block);
    qDebug() << "tcp mgr send byte data is " << block ;
}
