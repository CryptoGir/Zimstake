/*Copyright (C) 2014 Tim Schipper
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA*/

#ifndef IRCSERVER_H
#define IRCSERVER_H

#include <QtGui>
#include <QtNetwork>
#include <QtCore/QVariant>

#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QScrollBar>
#include <QListView>
#include <QTabWidget>
#include <QWidget>

#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QTime>
#include <QDateTime>

class IRCServer : public QTcpSocket {
    Q_OBJECT

public:
    IRCServer();
    QTextEdit *display;
    QListView *userList;
    QString nickname, ircserver, msgQuit;
    int port;
    QTabWidget *tab;
    QMap<QString, QTextEdit *> conversations;
    QSystemTrayIcon *tray;

    bool updateUsers;

    QString parseCommand(QString comm, bool ircserver = false);

    QWidget *parent;


signals:
    void nicknameChanged(QString newNickname);
    void joinTab();
    void tabJoined();

public slots:
    void readIRCServer();
    void errorSocket(QAbstractSocket::SocketError);
    void connected();
    void joinChannel();
    void sendData(QString txt);
    void join(QString chan);
    void messageTab(QString nick);
    void leave(QString chan);
    void ProcessLine(QString line, QString channel);
    void writeToUI(QString txt, QString destChan = "", QString msgTray = "");
    void writeToLog(QString txt);
    void removeUser(QString username);
    void addUser(QString username);
    void updateUserModel();
    void updateUsersList(QString chan = "");

    //void tabChanged(int index);
};

#endif // IRCSERVER_H
