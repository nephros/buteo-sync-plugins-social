TARGET = github-notifications-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=GithubNotificationsPlugin"
DEFINES += CLASSNAME_H=\\\"githubnotificationsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../github-common.pri)
include($$PWD/github-notifications.pri)

github_notifications_sync_profile.path = /etc/buteo/profiles/sync
github_notifications_sync_profile.files = $$PWD/github.Notifications.xml
github_notifications_client_plugin_xml.path = /etc/buteo/profiles/client
github_notifications_client_plugin_xml.files = $$PWD/github-notifications.xml
github_notifications_notification_xml.path = /usr/share/lipstick/notificationcategories/
github_notifications_notification_xml.files = $$PWD/x-nemo.social.github.notification.conf

HEADERS += githubnotificationsplugin.h
SOURCES += githubnotificationsplugin.cpp

OTHER_FILES += \
    github_notifications_sync_profile.files \
    github_notifications_client_plugin_xml.files \
    github_notifications_notification_xml.files

INSTALLS += \
    target \
    github_notifications_sync_profile \
    github_notifications_client_plugin_xml \
    github_notifications_notification_xml
