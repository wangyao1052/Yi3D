///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include "IpcServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QThread>

#include "wy3dQApplication.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {
// IPC frame format (big-endian):
//   0-3   magic       4B  "YI3D"
//   4     version     1B  0x01
//   5     msg_type    1B  0x01=request, 0x02=response
//   6-7   reserved    2B  must be 0
//   8-11  payload_len 4B  N (1..16MB)
//   12..  payload     NB  UTF-8 JSON
constexpr char kIpcMagic[4] = { 'Y', 'I', '3', 'D' };
constexpr quint8 kIpcVersion = 0x01;
constexpr quint8 kIpcMsgTypeRequest = 0x01;
constexpr quint8 kIpcMsgTypeResponse = 0x02;

QByteArray buildFrame(quint8 msgType, const QByteArray& payload)
{
    QByteArray frame;
    frame.reserve(12 + payload.size());
    frame.append(kIpcMagic, 4);
    frame.append(static_cast<char>(kIpcVersion));
    frame.append(static_cast<char>(msgType));
    frame.append(char(0));
    frame.append(char(0));

    const quint32 payloadLen = static_cast<quint32>(payload.size());
    frame.append(static_cast<char>((payloadLen >> 24) & 0xFF));
    frame.append(static_cast<char>((payloadLen >> 16) & 0xFF));
    frame.append(static_cast<char>((payloadLen >> 8) & 0xFF));
    frame.append(static_cast<char>(payloadLen & 0xFF));

    frame.append(payload);
    return frame;
}
}

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
{
    m_server.setMaxPendingConnections(1);
    connect(&m_server, &QTcpServer::newConnection, this, &IpcServer::onNewConnection);
    connect(&m_server, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError) {
        emit logMessage(QStringLiteral("IPC accept error: %1").arg(m_server.errorString()));
    });
}

bool IpcServer::start(quint16 port, const QHostAddress& host)
{
    if (m_server.isListening()) {
        return true;
    }

    if (!m_server.listen(host, port))
    {
        emit logMessage(QStringLiteral("IPC listen failed: %1").arg(m_server.errorString()));
        return false;
    }

    emit logMessage(QStringLiteral("IPC server listening at %1:%2")
                        .arg(m_server.serverAddress().toString())
                        .arg(m_server.serverPort()));
    return true;
}

void IpcServer::stop()
{
    closeActiveSocket();
    if (m_server.isListening())
    {
        m_server.close();
    }
}

bool IpcServer::isListening() const
{
    return m_server.isListening();
}

void IpcServer::setEventReceiver(QObject* receiver)
{
    m_eventReceiver = receiver;
}

void IpcServer::completeCurrentRequest(bool ok, const QString& message) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this,
                                  [this, ok, message]() { completeCurrentRequest(ok, message); },
                                  Qt::QueuedConnection);
        return;
    }

    if (!m_waitingResult) {
        return;
    }

    if (!m_activeSocket) {
        m_waitingResult = false;
        return;
    }
    sendResponseLine(ok ? makeOk(message) : makeError(message));
    m_waitingResult = false;
    closeActiveSocket();
}

void IpcServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (!socket) {
            continue;
        }

        if (isBusy()) {
            rejectBusy(socket);
            continue;
        }

        attachSocket(socket);
    }
}

void IpcServer::onSocketReadyRead()
{
    if (!m_activeSocket)
    {
        return;
    }

    m_buffer += m_activeSocket->readAll();

    if (m_waitingResult)
    {
        sendResponseLine(makeError(QStringLiteral("Busy")));
        closeActiveSocket();
        return;
    }

    if (m_buffer.size() < kHeaderBytes)
    {
        return;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(m_buffer.constData());
    if (bytes[0] != static_cast<unsigned char>(kIpcMagic[0]) ||
        bytes[1] != static_cast<unsigned char>(kIpcMagic[1]) ||
        bytes[2] != static_cast<unsigned char>(kIpcMagic[2]) ||
        bytes[3] != static_cast<unsigned char>(kIpcMagic[3]))
    {
        sendResponseLine(makeError(QStringLiteral("InvalidMagic")));
        closeActiveSocket();
        return;
    }

    const quint8 version = bytes[4];
    if (version != kIpcVersion)
    {
        sendResponseLine(makeError(QStringLiteral("UnsupportedVersion")));
        closeActiveSocket();
        return;
    }

    const quint8 msgType = bytes[5];
    if (msgType != kIpcMsgTypeRequest)
    {
        sendResponseLine(makeError(QStringLiteral("InvalidMessageType")));
        closeActiveSocket();
        return;
    }

    const quint16 reserved = (static_cast<quint16>(bytes[6]) << 8) | static_cast<quint16>(bytes[7]);
    if (reserved != 0)
    {
        sendResponseLine(makeError(QStringLiteral("InvalidReservedField")));
        closeActiveSocket();
        return;
    }

    const quint32 payloadLen = (static_cast<quint32>(bytes[8]) << 24) |
                               (static_cast<quint32>(bytes[9]) << 16) |
                               (static_cast<quint32>(bytes[10]) << 8) |
                               (static_cast<quint32>(bytes[11]));
    if (payloadLen == 0)
    {
        sendResponseLine(makeError(QStringLiteral("EmptyPayload")));
        closeActiveSocket();
        return;
    }
    if (payloadLen > kMaxPayloadBytes)
    {
        sendResponseLine(makeError(QStringLiteral("RequestTooLarge")));
        closeActiveSocket();
        return;
    }

    const qsizetype frameBytes = kHeaderBytes + static_cast<qsizetype>(payloadLen);
    if (m_buffer.size() < frameBytes)
    {
        return;
    }

    if (m_buffer.size() > frameBytes)
    {
        sendResponseLine(makeError(QStringLiteral("UnexpectedExtraData")));
        closeActiveSocket();
        return;
    }

    const QByteArray payload = m_buffer.mid(kHeaderBytes, static_cast<qsizetype>(payloadLen));
    m_buffer.clear();

    const bool shouldClose = processRequestPayload(payload);
    if (shouldClose)
    {
        closeActiveSocket();
    }
}

void IpcServer::onSocketDisconnected() {
    if (!m_activeSocket) {
        return;
    }

    m_activeSocket->deleteLater();
    m_activeSocket = nullptr;
    m_buffer.clear();
    m_waitingResult = false;
}

QString IpcServer::normalizePath(const QString& path) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return QDir::cleanPath(canonical);
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

QString IpcServer::sanitizeForLine(const QString& text)
{
    QString out = text;
    out.replace("\r\n", "\\n");
    out.replace('\n', "\\n");
    out.replace('\r', "\\r");
    return out;
}

bool IpcServer::isBusy() const {
    return m_activeSocket && m_activeSocket->state() != QAbstractSocket::UnconnectedState;
}

void IpcServer::attachSocket(QTcpSocket* socket) {
    m_activeSocket = socket;
    m_buffer.clear();

    connect(socket, &QTcpSocket::readyRead, this, &IpcServer::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &IpcServer::onSocketDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit logMessage(QStringLiteral("IPC socket error: %1")
                            .arg(m_activeSocket ? m_activeSocket->errorString() : QStringLiteral("unknown")));
    });
}

void IpcServer::rejectBusy(QTcpSocket* socket)
{
    if (!socket) {
        return;
    }

    const QByteArray payload = makeError(QStringLiteral("Busy")).toUtf8();
    socket->write(buildFrame(kIpcMsgTypeResponse, payload));
    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}

void IpcServer::closeActiveSocket() {
    if (!m_activeSocket) {
        return;
    }

    if (m_activeSocket->state() != QAbstractSocket::UnconnectedState) {
        m_activeSocket->disconnectFromHost();
    }

    m_activeSocket->deleteLater();
    m_activeSocket = nullptr;
    m_buffer.clear();
}

bool IpcServer::processRequestPayload(const QByteArray& payload)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        sendResponseLine(makeError(QStringLiteral("InvalidJsonRequest")));
        return true;
    }

    const QJsonObject json = doc.object();
    const QString command = json.value(QStringLiteral("commnad")).toString().trimmed();
    return this->onCommand(command, json);
}

bool IpcServer::onCommand(const QString& command, const QJsonObject& json)
{
    if (command == QStringLiteral("ping"))
    {
        return onCmdPing(json);
    }
    else if (command == QStringLiteral("script"))
    {
        return onCmdScript(json);
    }
    else if (command == QStringLiteral("command"))
    {
        return onCmdCommand(json);
    }
    else
    {
        sendResponseLine(makeError(QStringLiteral("UnsupportedCommand:%1").arg(command)));
    }
    return true;
}

bool IpcServer::onCmdPing(const QJsonObject& json)
{
    sendResponseLine(makeOk(QStringLiteral("pong")));
    return true;
}

bool IpcServer::onCmdScript(const QJsonObject& json)
{
    const QString path = json.value(QStringLiteral("argument")).toString().trimmed();
    if (path.isEmpty())
    {
        sendResponseLine(makeError(QStringLiteral("MissingArgumentField")));
        return true;
    }

    const QString absolutePath = normalizePath(path);

    if (!QFileInfo::exists(absolutePath)) 
    {
        sendResponseLine(makeError(QStringLiteral("ScriptFileNotFound:%1").arg(absolutePath)));
        return true;
    }

    if (!m_eventReceiver)
    {
        sendResponseLine(makeError(QStringLiteral("EventReceiverNotConfigured")));
        return true;
    }

    m_waitingResult = true;
    QCoreApplication::postEvent(m_eventReceiver, new RunPythonScriptEvent(absolutePath.toStdString()));
    return false;
}

bool IpcServer::onCmdCommand(const QJsonObject& json)
{
    const QString cmdName = json.value(QStringLiteral("argument")).toString().trimmed();
    if (cmdName.isEmpty())
    {
        sendResponseLine(makeError(QStringLiteral("MissingArgumentField")));
        return true;
    }

    m_waitingResult = true;
    QCoreApplication::postEvent(m_eventReceiver, new RunGuiCommandEvent(cmdName.toStdString()));
    return false;
}

void IpcServer::sendResponseLine(const QString& line) {
    if (!m_activeSocket) {
        return;
    }

    const QByteArray payload = sanitizeForLine(line).toUtf8();
    m_activeSocket->write(buildFrame(kIpcMsgTypeResponse, payload));
    m_activeSocket->flush();
}

QString IpcServer::makeOk(const QString& message) {
    return QStringLiteral("OK|%1").arg(sanitizeForLine(message));
}

QString IpcServer::makeError(const QString& error) {
    return QStringLiteral("ERROR|%1").arg(sanitizeForLine(error));
}

} // namespace wy3dApp
