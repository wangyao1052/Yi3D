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

#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <wy3dApp/wy3dAppDefs.h>

class QJsonObject;

namespace wy3dApp {

class IpcServer : public QObject {
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);

    bool start(quint16 port = 17999, const QHostAddress& host = QHostAddress::LocalHost);
    void stop();

    bool isListening() const;

    void setEventReceiver(QObject* receiver);

signals:
    void logMessage(const QString& message);

public slots:
    void completeCurrentRequest(bool ok, const QString& message);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    static QString normalizePath(const QString& path);
    static QString sanitizeForLine(const QString& text);

    bool isBusy() const;

    void attachSocket(QTcpSocket* socket);
    void rejectBusy(QTcpSocket* socket);
    void closeActiveSocket();

    bool processRequestPayload(const QByteArray& payload);
    void sendResponseLine(const QString& line);

    bool onCommand(const QString& command, const QJsonObject& json);
    bool onCmdPing(const QJsonObject& json);
    bool onCmdScript(const QJsonObject& json);
    bool onCmdCommand(const QJsonObject& json);

    static QString makeOk(const QString& message);
    static QString makeError(const QString& error);

private:
    static constexpr qsizetype kHeaderBytes = 12;
    static constexpr quint32 kMaxPayloadBytes = 16 * 1024 * 1024;

    QTcpServer m_server;
    QPointer<QTcpSocket> m_activeSocket;
    QByteArray m_buffer;

    QPointer<QObject> m_eventReceiver;
    bool m_waitingResult = false;
};

}
