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

#include "ircserver.h"
#include "util.h"
#include "guiutil.h"
#include "chatwindow.h"

QStringList users;

bool delist = true;
bool requestSend = false;

IRCServer::IRCServer() {
    connect(this, SIGNAL(readyRead()), this, SLOT(readIRCServer()));
    connect(this, SIGNAL(connected()), this, SLOT(connected()));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorSocket(QAbstractSocket::SocketError)));
}

void IRCServer::errorSocket(QAbstractSocket::SocketError error) {
    switch (error) {
        case QAbstractSocket::HostNotFoundError:
            display->append(tr("<em>ERROR : can't find freenode server.</em>"));
            break;
        case QAbstractSocket::ConnectionRefusedError:
            display->append(tr("<em>ERROR : server refused connection</em>"));
            break;
        case QAbstractSocket::RemoteHostClosedError:
            display->append(tr("<em>ERROR : server cut connection</em>"));
            break;
        default:
            display->append(tr("<em>ERROR : ") + this->errorString() + tr("</em>"));
    }
}

// Send user and nick on connected

void IRCServer::connected() {
    
    writeToUI("`--- ---'");
    writeToUI("`--- Welcome to the Thunderdome ---'");
    writeToUI("`--- ---'");
    
    sendData("USER " + nickname + " localhost " + ircserver + " :" + nickname);
    sendData("NICK " + nickname);
    
    messageTab(nickname);
}

// Switch initial channel based on -testirc flag

void IRCServer::joinChannel() {
    if (GetBoolArg("-testirc")) {
        join("#zimstake-testing");
    } else {
        join("#ProjectZIM");
    }
}

// IRC Logger when in -testirc mode

void IRCServer::writeToLog(QString txt) {
    if (GetBoolArg("-testirc")) {

        QString path(QString::fromStdString(GetDataDir(false).string()));
        QFile file(path + "/irc.log");

        if (!file.open(QIODevice::Append | QIODevice::Text))
            return;

        QTextStream log(&file);
        log << QTime::currentTime().toString() + " " + txt << endl;
    }
}

// Process server response

void IRCServer::ProcessLine(QString line, QString channel) {

    QStringList validCommands;
    QRegExp linematch;
    QRegExp actionmatch;

    linematch.setPattern("^(:?([A-Za-z0-9_\\-\\\\\\[\\]{}^`|.]+)?!?(\\S+)? )?([A-Z0-9]+) ?([A-Za-z0-9_\\-\\\\\\[\\]{}^`|.*#]+)?( [@=] ([#&][^\\x07\\x2C\\s]{0,200}))?([ :]+)?([\\S ]+)?$");
    
    validCommands << "353" << "376" << "JOIN" << "KICK" << "MODE";
    validCommands << "NICK" << "NOTICE" << "PART" << "PING" << "PRIVMSG";
    validCommands  << "QUIT" << "TOPIC" << "372" << "432" << "433";
    validCommands  << "332";
    
    actionmatch.setPattern("\\001ACTION(.+)\\001");

    linematch.isMinimal();
    actionmatch.isMinimal();

    int pos = linematch.indexIn(line);

    if (pos > -1) {

        writeToLog("[RCV] " + line);

        QStringList list;
        QStringList splitted;
        
        QString sender = linematch.cap(2);
        QString userhost = linematch.cap(3);
        QString command = linematch.cap(4);
        QString receiver = linematch.cap(5);
        QString destination = linematch.cap(7);
        QString content = linematch.cap(9);

        QString response;
        QString username;
        QString reason;
        QString channel;
        QString topic;

        switch (validCommands.indexOf(command)) {
            case 0:
                // 353 (Names)
                if (delist == true) users.clear();
                list = content.split(" ");

                for (int i = 0; i < list.count(); i++) {
                    users.append(list.at(i));
                }

                if (list.count() < 53) delist = true;
                else delist = false;

                updateUserModel();
                break;
            case 1:
                // 376 (End of MOTD)
                joinChannel();
                break;
            case 2:
                // JOIN
                response = GUIUtil::HtmlEscape("Join: " + sender + " (" + userhost + ")");
                addUser(sender);
                writeToUI(response, receiver);
                break;
            case 3:
                // KICK
                splitted = content.split(" :");
                username = splitted.at(0);
                reason = splitted.at(1);

                removeUser(username);

                response = GUIUtil::HtmlEscape(sender + " kicked " + username + ". Reason: " + reason);
                writeToUI(response, receiver);
                break;
            case 4:
                // MODE
                response = GUIUtil::HtmlEscape(sender + " sets mode: " + content);
                if (receiver.startsWith("#")) {
                    updateUsersList(receiver);
                    writeToUI(response, receiver);
                } else {
                    writeToUI(response);
                }
                break;
            case 5:
                // NICK
                response = GUIUtil::HtmlEscape(sender + " is now known as `" + content);
                removeUser(sender);
                addUser(content);
                writeToUI(response);
                break;
            case 6:
                // NOTICE
                writeToUI(GUIUtil::HtmlEscape(content));
                break;
            case 7:
                // PART
                response = GUIUtil::HtmlEscape("Parts: " + sender + " (" + userhost + ")");
                removeUser(sender);
                writeToUI(response, receiver);
                break;
            case 8:
                // PING
                response = "PONG " + content;
                sendData(response);
                break;
            case 9:
                // PRIVMSG
                int posact;
                posact = actionmatch.indexIn(content);
                if (posact > -1) {
                    QString actionmsg = actionmatch.cap(1);
                    response = GUIUtil::HtmlEscape(sender + " " + actionmsg.trimmed());
                } else {
                    if (receiver == nickname) {
                        response = GUIUtil::HtmlEscape(sender + " -> you: " + content);
                    } else {
                        response = GUIUtil::HtmlEscape("<" + sender + "> " + content);
                    }
                }                
                if (receiver.startsWith("#") || receiver == nickname) {
                    writeToUI(response, receiver);
                } else {            
                    writeToUI(response);
                }
                break;
            case 10:
                // QUIT
                response = GUIUtil::HtmlEscape("Quit: " + sender + " (" + userhost + ")");
                removeUser(sender);
                writeToUI(response);
                break;
            case 11:
                // TOPIC
                response = GUIUtil::HtmlEscape(sender + " changes topic to '" + content + "'");
                writeToUI(response, receiver);
                break;
            case 12:
                // 372 (MOTD)
                writeToUI(GUIUtil::HtmlEscape(content));
                break;
            case 13:
                // 432 (Invalid nickname)
                response = "ERROR: " + content;
                writeToUI(GUIUtil::HtmlEscape(response));
                break;
            case 14:
                // 433 (Nick already in use)
                response = "Nickname is already in use, please choose another. Type /nick YOURNEWNAME";
                writeToUI(GUIUtil::HtmlEscape(response));
                break;  
            case 15:
                // 332 (Topic on join)
                splitted = content.split(" :");
                channel = splitted.at(0);
                topic = splitted.at(1);
                
                response = GUIUtil::HtmlEscape("Topic for " + channel + " is '" + topic + "'");
                writeToUI(response, channel);
                break;
            default:
                // Do nothing
                break;
        }

    } else {
        writeToLog("[ERR] " + line);
    }
}

// Read response from server

void IRCServer::readIRCServer() {

    QString message = QString::fromUtf8(this->readAll());
    QString currentChan = tab->tabText(tab->currentIndex());

    QStringList list = message.split("\r\n");

    foreach(QString msg, list) {
        if (msg.trimmed() != "") {
            ProcessLine(msg, currentChan);
        }
    }
}

// Send request to server

void IRCServer::sendData(QString txt) {
    if(txt.size() > 0) {
        writeToLog("[SND] " + txt);

        if (this->state() == QAbstractSocket::ConnectedState) {
            this->write((txt + "\r\n").toUtf8());
        }
    }
}

QString IRCServer::parseCommand(QString command, bool ircserver) {
    
    QString destChan = tab->tabText(tab->currentIndex());
    
    if (command.startsWith("/")) {
        
        QRegExp cmd;
        QString pref;
        QString msg;
        
        cmd.setPattern("^/([A-Za-z]+)\\s(.+)$");
        cmd.isMinimal();
        
        int pos = cmd.indexIn(command);

        if (pos > -1) {
            pref = cmd.cap(1);
            msg = cmd.cap(2);
        }

        if (pref == "me" && (destChan.startsWith("#") || destChan == nickname)) {
            writeToUI(GUIUtil::HtmlEscape(nickname) + " " + GUIUtil::HtmlEscape(msg), destChan);
            return "PRIVMSG " + destChan + " :\001ACTION " + msg + "\001";
        } else if (pref == "msg") {
            
            QStringList args = msg.split(" ");
            QString receiver = args.takeAt(0);
            QString message = args.join(" ");
            
            writeToUI("you -> " + receiver + ": " + message, nickname);
            
            return "PRIVMSG " + receiver + " :" + message;
        } else if (pref == "quit") {
            return "QUIT (Zimstake Wallet IRC v1.0.1.5)";
        } else if (pref == "nick") {
            emit nicknameChanged(msg);
            return "NICK " + msg;
        }  else
            // Do nothing
            return "";
        
    } else if (!ircserver && destChan.startsWith("#")) {
        
        if (command.endsWith("<br />"))
            command = command.remove(QRegExp("<br />$"));

        QString htmlOut = "<span style=\"color:#B5D1EE;font-weight:600;\">&lt;" + nickname + "&gt;</span> " + command;
        
        writeToUI(htmlOut, destChan);

        if (!command.startsWith(":"))
            command.insert(0, ":");
        
        QString request("PRIVMSG " + destChan + " " + command);
        return request;
    } else {
        // Do nothing
        return "";
    }
}

void IRCServer::join(QString chan) {
    emit joinTab();
    QTextEdit *textEdit = new QTextEdit;
    int index = tab->insertTab(tab->currentIndex() + 2, textEdit, chan);
    tab->setTabToolTip(index, ircserver);
    tab->setCurrentIndex(index);

    textEdit->setReadOnly(true);

    conversations.insert(chan, textEdit);

    sendData("JOIN " + chan);

    emit tabJoined();
}

void IRCServer::messageTab(QString nick) {
    emit joinTab();
    
    QTextEdit *textEdit = new QTextEdit;
    int index = tab->insertTab(tab->currentIndex() + 1, textEdit, nick);
    
    tab->setTabToolTip(index, "irc.freenode.net");
    textEdit->setReadOnly(true);

    conversations.insert(nick, textEdit);

    emit tabJoined();
}

void IRCServer::leave(QString chan) {
    sendData(parseCommand("/part " + chan));
}

void IRCServer::writeToUI(QString txt, QString destChan, QString msgTray) {
    QTime time = QTime::currentTime();
    QString timeString = "[" + time.toString("hh:mm")  + "] ";
    
    if (destChan != "") {
        conversations[destChan]->setHtml(conversations[destChan]->toHtml() + timeString + txt);
        QScrollBar *sb = conversations[destChan]->verticalScrollBar();
        sb->setValue(sb->maximum());
    } else if (txt.startsWith("#")) {
        QString dest = txt.split(" ").first();
        QStringList list = txt.split(" ");
        list.removeFirst();
        txt = list.join(" ");
        conversations[dest]->setHtml(conversations[dest]->toHtml() + timeString + txt);
        QScrollBar *sb = conversations[dest]->verticalScrollBar();
        sb->setValue(sb->maximum());
    } else {
        txt.replace("\r\n", "<br />");
        display->setHtml(display->toHtml() + timeString + txt);
        QScrollBar *sb = display->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

// Update userlist

void IRCServer::updateUserModel() {
    QStringListModel *model;
    
    users.sort();
    
    model = new QStringListModel(users);
    userList->setModel(model);
    userList->update();
}

// Send NAMES request to server

void IRCServer::updateUsersList(QString chan) {
    if (chan.startsWith("#")) {
        sendData("NAMES " + chan);
    }
}

// Remove user from userlist

void IRCServer::removeUser(QString username) {
    int length = username.size() - 1;
    QString subString = username.mid(1, length);

    for (int i = 0; i < users.size(); ++i) {
        if (users.at(i) == username || users.at(i) == subString)
            users.removeAt(i);
    }
    updateUserModel();
}

// Add user to userlist

void IRCServer::addUser(QString username) {
    if (username != nickname) {
        users.append(username);
        updateUserModel();
    }
}
