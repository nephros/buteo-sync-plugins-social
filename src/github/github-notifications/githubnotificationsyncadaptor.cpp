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

#include "githubnotificationsyncadaptor.h"
#include "trace.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QUrlQuery>
#include <QDebug>

static const int OLD_NOTIFICATION_LIMIT_IN_DAYS = 21;
static const int NOTIFICATIONS_LIMIT = 30;

GithubNotificationSyncAdaptor::GithubNotificationSyncAdaptor(QObject *parent)
    : GithubDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Notifications, parent)
{
    setInitialActive(true);
}

GithubNotificationSyncAdaptor::~GithubNotificationSyncAdaptor()
{
}

QString GithubNotificationSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("github-microblog");
}

void GithubNotificationSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    m_db.removeNotifications(oldId);
    m_db.sync();
    m_db.wait();
}

void GithubNotificationSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestNotifications(accountId, accessToken);
}

void GithubNotificationSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId);
    if (syncAborted()) {
        qCDebug(lcSocialPlugin) << "sync aborted, skipping finalize of VK Notifications from account:" << accountId;
    } else {
        m_db.purgeOldNotifications(OLD_NOTIFICATION_LIMIT_IN_DAYS);

        m_db.sync();
        m_db.wait();

        setLastSuccessfulSyncTime(accountId);
    }
}

void GithubNotificationSyncAdaptor::requestNotifications(int accountId, const QString &accessToken, const QString &until, const QString &pagingToken)
{
    // TODO: result paging
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("X-GitHub-Api-Version")), QStringLiteral("2022-11-28"))); // API version
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("accept")), QString(QLatin1String("application/vnd.github+json"))));
    //queryItems.append(QPair<QString, QString>(QString(QLatin1String("all")), QString(QLatin1String("false"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("all")), QString(QLatin1String("true"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("participating")), QString(QLatin1String("true"))));
    QDateTime since = lastSuccessfulSyncTime(accountId);
    if (!since.isValid()) {
            int sinceSpan = m_accountSyncProfile
                    ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                    : 7;
            since = QDateTime::currentDateTime().addDays(-1 * sinceSpan).toUTC();
    }
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("since")), QString::number(since.toTime_t())));

    QUrl url(QStringLiteral("https://api.github.com/notifications")); // NOTE: According to https://github.com/orgs/community/discussions/13056, not in GraphQL (yet)
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        qCWarning(lcSocialPlugin) << "unable to request notifications from Github account with id" << accountId;
    }
}

void GithubNotificationSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonArray data = parseJsonArrayReplyData(replyData, &ok);

    // https://docs.github.com/en/rest/activity/notifications?apiVersion=2022-11-28
    if (!isError && ok && data.count() > 0) {

        foreach (const QJsonValue &entry, data) {
            QJsonObject object = entry.toObject();
            if (!object.isEmpty()) {
                m_notificationsToAdd.append(NotificationData(accountId, object, profileValues));
            } else {
                qCDebug(lcSocialPlugin) << "notification object empty; skipping";
            }
        }
    } else {
        // error occurred during request.
        qCWarning(lcSocialPlugin) << "error: unable to parse notification data from request with account:" << accountId <<
                          "got:" << QString::fromUtf8(replyData);
    }

}

QDateTime GithubNotificationSyncAdaptor::lastSuccessfulSyncTime(int accountId)
{
    QDateTime result;
    QString settingsFileName = QString::fromLatin1("%1/%2/ghnotif.ini")
            .arg(PRIVILEGED_DATA_DIR)
            .arg(SYNC_DATABASE_DIR);
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    uint timestamp = settingsFile.value(QString::fromLatin1("%1-last-successful-sync-time").arg(accountId)).toUInt();
    if (timestamp > 0) {
        result = QDateTime::fromTime_t(timestamp);
    }
    return result;
}

void GithubNotificationSyncAdaptor::setLastSuccessfulSyncTime(int accountId)
{
    QDateTime currentTime = QDateTime::currentDateTime().toUTC();
    QString settingsFileName = QString::fromLatin1("%1/%2/ghnotif.ini")
            .arg(PRIVILEGED_DATA_DIR)
            .arg(SYNC_DATABASE_DIR);
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    settingsFile.setValue(QString::fromLatin1("%1-last-successful-sync-time").arg(accountId),
                          QVariant::fromValue<uint>(currentTime.toTime_t()));
    settingsFile.sync();
}
