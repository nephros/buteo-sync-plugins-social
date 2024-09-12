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

#include "rssfeedsyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
//#include <QtCore/QJsonValue>
#include <QtCore/QUrlQuery>

#include <QDomDocument>

RSSFeedSyncAdaptor::RSSFeedSyncAdaptor(QObject *parent)
    : RSSDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Posts, parent)
{
    setInitialActive(m_db.isValid());
}

RSSFeedSyncAdaptor::~RSSFeedSyncAdaptor()
{
}

/*
void RSSFeedSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    m_db.removePosts(oldId);
    m_db.commit();
    m_db.wait();

    // manage image cache. Social media feed UI caches feed images
    // and maintains bindings between source and cached image in SocialImageDatabase.
    // purge cached images belonging to this account.
    purgeCachedImages(&m_imageCacheDb, oldId);
}
*/

QString RSSFeedSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("rssfeed-microblog");
}

void RSSFeedSyncAdaptor::beginSync(int accountId)
{
    requestPosts(accountId);
}

void RSSFeedSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    if (syncAborted()) {
        qCInfo(lcSocialPlugin) << "sync aborted, won't commit database changes";
    } else {
        m_db.commit();
        m_db.wait();

        // manage image cache. Social media feed UI caches feed images
        // and maintains bindings between source and cached image in SocialImageDatabase.
        // purge cached images older than four weeks.
        purgeExpiredImages(&m_imageCacheDb, accountId);
    }
}

void RSSFeedSyncAdaptor::requestPosts(int accountId, const QString &feedUrl, const QString &sinceId)
{
    QUrl url(feedUrl);
    QNetworkRequest nreq(url);
    QNetworkReply *reply = m_networkAccessManager->get(nreq);

    if (reply) {
        reply->setProperty("accountId", accountId);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedPostsHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        qCWarning(lcSocialPlugin) << "unable to request elements from RSS feed with id" << accountId;
    }
}

void RSSFeedSyncAdaptor::finishedPostsHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("rssfeed"),
                                           SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts),
                                           accountId);
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    QDomDocument feed;
    bool ok = feed.setContent(replyData);

    if (ok) {
        QDomElement root = feed.firstChildElement(QStringLiteral("rss"));
        QDomElement chan = root.firstChildElement(QStringLiteral("channel"));
        QDomElement item = chan.firstChildElement(QStringLiteral("item"));

        if (item.isNull()) {
            qCDebug(lcSocialPlugin) << "no feed posts received for account" << accountId;
            decrementSemaphore(accountId);
            return;
        }

        QString feedName;
        QString feedUrl;
        QString feedDesc;
        if (!chan.isNull()) {
            // the following elements of channel are required:
            feedName = chan.firstChildElement(QStringLiteral("title")).text();
            feedUrl  = chan.firstChildElement(QStringLiteral("link")).text();
            feedDesc = chan.firstChildElement(QStringLiteral("description")).text();
        }

        m_db.removePosts(accountId); // purge old

        for (; !item.isNull(); item = item.nextSiblingElement("item")) {

            // the following elements of item are required:
            QString itemName = item.firstChildElement(QStringLiteral("title")).text();
            QString itemUrl  = item.firstChildElement(QStringLiteral("link")).text();
            QString itemDesc = item.firstChildElement(QStringLiteral("description")).text();

            // these are optional, but we want them in the database:
            QString itemDate = item.firstChildElement(QStringLiteral("pubDate")).text();

            // these are the fields we eventually need to fill out:
            QList<QPair<QString, SocialPostImage::ImageType> > imageList;

            // RSS enclosure is for media:
            QDomNodeList mediaList = item.elementsByTagName(QStringLiteral("enclosure"));
            if (!mediaList.isEmpty()) {
                for (int i = 0; i < mediaList.length(); ++i) {
                    QDomElement mediaElement = mediaList.item(i).toElement();
                    if (mediaElement.isNull() && mediaElement.hasAttribute(QStringLiteral("url"))) {
                        QString ts = mediaElement.attribute(QStringLiteral("type"));

                        SocialPostImage::ImageType type = SocialPostImage::Invalid;
                        if (ts.startsWith(QStringLiteral("image"))
                            type = SocialPostImage::Photo;
                        if (ts.startsWith(QStringLiteral("video"))
                            type = SocialPostImage::Video;
                        imageList.append(qMakePair<QString, SocialPostImage::ImageType>(mediaElement.attribute(QStringLiteral("url")), type));
                    }
                }
            }

            // We always purge, so even if we've synced it in the past, we need it.
            // Check to see if we need to post it to the events feed
            int sinceSpan = m_accountSyncProfile
                          ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                          : 7;
            if (itemDate.daysTo(QDateTime::currentDateTime()) > sinceSpan) {
                qCDebug(lcSocialPlugin) << "feed for account" << accountId <<
                                  "is more than" << sinceSpan << "days old:" <<
                                  itemDate.toString(Qt::ISODate) << body;
            } else {
                // libsocialcache/src/lib/rssfeedpostsdatabase.h:
                // void addRSSFeedPost(const QString &identifier, const QString &name, const QString &body,
                //                        const QDateTime &timestamp,
                //                        const QString &icon,
                //                        const QList<QPair<QString, SocialPostImage::ImageType> > &images,
                //                        const QString &feedName,
                //                        int account);
                //
                m_db.addRSSFeedPost(postId, itemName, itemDesc, itemDate, icon, imageList,
                                    feedName, accountId);
            }
        }
    } else {
        // error occurred during request.
        qCWarning(lcSocialPlugin) << "unable to parse feed data from request with account" << accountId << "," <<
                          "got:" << QString::fromLatin1(replyData.constData());
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
