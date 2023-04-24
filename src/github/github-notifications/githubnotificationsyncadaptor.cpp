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

static const int NOTIFICATIONS_LIMIT = 30;

GithubNotificationSyncAdaptor::GithubNotificationSyncAdaptor(QObject *parent)
    : GithubDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Notifications, parent)
{
    //setInitialActive(true);
    setInitialActive(m_db.isValid());
}

GithubNotificationSyncAdaptor::~GithubNotificationSyncAdaptor()
{
}

QString GithubNotificationSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("github-posts");
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
        qCDebug(lcSocialPlugin) << "sync aborted, skipping finalize of Github Notifications from account:" << accountId;
    } else {

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
    //FIXME: format not accepted upstream...
    QString sincestr = since.toString(Qt::ISODate);
    qCInfo(lcSocialPlugin) << "setting since to" << sincestr;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("since")), sincestr));

    QUrl url(QStringLiteral("https://api.github.com/notifications"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest request = QNetworkRequest(url);
    request.setRawHeader(QString(QLatin1String("Accept")).toUtf8(),
                    QString(QLatin1String("application/vnd.github+json")).toUtf8());
    request.setRawHeader(QString(QLatin1String("X-GitHub-Api-Version")).toUtf8(),
                    QString(QLatin1String("2022-11-28")).toUtf8());
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                    QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
    QNetworkReply *reply = m_networkAccessManager->get(request);

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
                // NB: the spec has this as a string! Also, conversion from JSON->String->UInt is very finicky.
                quint32 tid      = object.value(QStringLiteral("id")).toString().toULong();
                if (tid == 0) {
                        qCWarning(lcSocialPlugin) << "Error: id is zero, (wither not a number or conversion failed) skipping entry.";
                        continue;
                }
                // repo data:
                QJsonObject r    = object.value(QStringLiteral("repository")).toObject();
                QString from     = r.value(QStringLiteral("full_name")).toString();
                QString repo     = QLatin1String(QJsonDocument(r).toJson(QJsonDocument::Compact)); // full metadata
                // Owner data
                QJsonObject ow   = r.value(QStringLiteral("owner")).toObject();
                QString avatar   = ow.value(QStringLiteral("avatar_url")).toString();

                // subject/message
                QJsonObject subj = object.value(QStringLiteral("subject")).toObject();
                QString title    = subj.value(QStringLiteral("title")).toString();
                QString type     = subj.value(QStringLiteral("type")).toString();
                // which URL to use?
                // we have:
                //  - .url --> go to notification thread
                //  - .subject.url --> go to subject, e.g. URL of an issue
                //  - .subject.latest_comment_url --> at least for issues...
                //  - .repository.url --> URL of repo which "sent" this notification
                //  - .repository.owner.html_url --> URL of user who "sent" this notification
                QString url      = subj.value(QStringLiteral("url")).toString();

                QString reason = object.value(QStringLiteral("reason")).toString();
                bool unread    = object.value(QStringLiteral("unread")).toBool();
                QDateTime updated = QDateTime::fromString(object.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);

                qCDebug(lcSocialPluginTrace) << "Adding Github notification:" << accountId << tid << from << title;

                m_db.addGithubNotification(accountId,
                                           tid,
                                           type,
                                           title,
                                           from,
                                           reason,
                                           unread,
                                           repo,
                                           avatar,
                                           url,
                                           updated);
            } else {
                qCDebug(lcSocialPlugin) << "notification object empty; skipping";
            }
        }
        m_db.sync();
        m_db.wait();
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
