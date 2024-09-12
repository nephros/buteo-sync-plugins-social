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

#include "rssfeeddatatypesyncadaptor.h"
#include "trace.h"

#include <QtCore/QDebug>

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUuid>
#include <QtCore/qmath.h>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

// libaccounts-qt5
#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>
#include <Accounts/AccountService>

RSSDataTypeSyncAdaptor::RSSDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("rssfeed", dataType, 0, parent), m_triedLoading(false)
{
}

RSSDataTypeSyncAdaptor::~RSSDataTypeSyncAdaptor()
{
}

void RSSDataTypeSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    if (dataTypeString != SocialNetworkSyncAdaptor::dataTypeName(m_dataType)) {
        qCWarning(lcSocialPlugin) << "RSSFeed" << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                          "sync adaptor was asked to sync" << dataTypeString;
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    setStatus(SocialNetworkSyncAdaptor::Busy);
    updateDataForAccount(accountId);
    qCDebug(lcSocialPlugin) << "successfully triggered sync with profile:" << m_accountSyncProfile->name();
}

void RSSDataTypeSyncAdaptor::updateDataForAccount(int accountId)
{
    Accounts::Account *account = Accounts::Account::fromId(m_accountManager, accountId, this);
    if (!account) {
        qCWarning(lcSocialPlugin) << "existing account with id" << accountId << "couldn't be retrieved";
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // will be decremented by either signOnError or signOnResponse.
    incrementSemaphore(accountId);
}


void RSSDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();

    qCWarning(lcSocialPlugin) << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                      "request with account" << accountId <<
                      "experienced error:" << err <<
                      "HTTP:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    reply->setProperty("isError", QVariant::fromValue<bool>(true));
}

void RSSDataTypeSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
{
    QString sslerrs;
    foreach (const QSslError &e, errs) {
        sslerrs += e.errorString() + "; ";
    }
    if (errs.size() > 0) {
        sslerrs.chop(2);
    }
    qCWarning(lcSocialPlugin) << SocialNetworkSyncAdaptor::dataTypeName(m_dataType) <<
                      "request with account" << sender()->property("accountId").toInt() <<
                      "experienced ssl errors:" << sslerrs;
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    sender()->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.
}

