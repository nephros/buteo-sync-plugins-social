/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jolla.com>
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

#include "githubnotificationsplugin.h"
#include "githubnotificationsyncadaptor.h"
#include "socialnetworksyncadaptor.h"

#include <QTranslator>
#include <QCoreApplication>

GithubNotificationsPlugin::GithubNotificationsPlugin(const QString& pluginName,
                             const Buteo::SyncProfile& profile,
                             Buteo::PluginCbInterface *callbackInterface)
    : SocialdButeoPlugin(pluginName, profile, callbackInterface,
                         QStringLiteral("github"),
                         SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications))
{
    QString translationPath("/usr/share/translations/");
    // QTranslator life-cycle: owned by ButeoSocial and removed by its own destructor
    QTranslator *engineeringEnglish = new QTranslator(this);
    engineeringEnglish->load("lipstick-jolla-home-github-notif_eng_en", translationPath);
    QCoreApplication::instance()->installTranslator(engineeringEnglish);

    QTranslator *translator = new QTranslator(this);
    translator->load(QLocale(), "lipstick-jolla-home-github-notif", "-", translationPath);
    QCoreApplication::instance()->installTranslator(translator);
}

GithubNotificationsPlugin::~GithubNotificationsPlugin()
{
}

SocialNetworkSyncAdaptor *GithubNotificationsPlugin::createSocialNetworkSyncAdaptor()
{
    return new GithubNotificationSyncAdaptor(this);
}


Buteo::ClientPlugin* GithubNotificationsPluginLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new GithubNotificationsPlugin(pluginName, profile, cbInterface);
}

