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

#ifndef RSSDATATYPESYNCADAPTOR_H
#define RSSDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

namespace Accounts {
    class Account;
}
/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from RSS Feeds.
*/

class RSSDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    RSSDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~RSSDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString, int accountId);

protected:
    virtual void updateDataForAccount(int accountId);
    virtual void beginSync(int accountId) = 0;
protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private:
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
};

#endif // RSSDATATYPESYNCADAPTOR_H
