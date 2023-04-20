/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
 **
 ****************************************************************************/

#ifndef GITHUBNOTIFICATIONSYNCADAPTOR_H
#define GITHUBNOTIFICATIONSYNCADAPTOR_H

#include "githubdatatypesyncadaptor.h"
#include <socialcache/githubnotificationsdatabase.h>

#include <QList>
#include <QJsonObject>

class Notification;

class GithubNotificationSyncAdaptor : public GithubDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    GithubNotificationSyncAdaptor(QObject *parent);
    ~GithubNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing GithubDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(),
                              const QString &pagingToken = QString());
    QDateTime lastSuccessfulSyncTime(int accountId);
    void setLastSuccessfulSyncTime(int accountId);

private Q_SLOTS:
    void finishedHandler();

private:
    //void saveGithubNotificationFromObject(int accountId, const QJsonObject &notif);
    struct NotificationData {
        NotificationData() : accountId(0) {}
        NotificationData(int accountId, const QJsonObject &notification)
            : accountId(accountId), notification(notification) {}
        int accountId;
        QJsonObject notification;
    };
    GithubNotificationsDatabase m_db;
};

#endif // GITHUBNOTIFICATIONSYNCADAPTOR_H
