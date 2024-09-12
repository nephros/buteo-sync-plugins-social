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

#ifndef RSSFEEDSYNCADAPTOR_H
#define RSSFEEDSYNCADAPTOR_H

#include "rssfeeddatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/rssfeedpostsdatabase.h>
#include <socialcache/socialimagesdatabase.h>

class RSSFeedSyncAdaptor : public RSSDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    RSSFeedSyncAdaptor(QObject *parent);
    ~RSSFeedSyncAdaptor();

    QString syncServiceName() const;

protected:
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId);
    void finalize(int accountId);

private:
    void requestPosts(int accountId,
                    const QString &url = QString(), 
                    const QString &sinceId = QString());

private Q_SLOTS:
    void finishedPostsHandler();

private:
    RSSFeedDatabase m_db;
    SocialImagesDatabase m_imageCacheDb;
    QMap<int, QString> m_accountProfileImage;
};

#endif // RSSFEEDSYNCADAPTOR_H
