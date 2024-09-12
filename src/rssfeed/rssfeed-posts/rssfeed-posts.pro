TARGET = rssfeed-posts-client

include($$PWD/../../common.pri)
include($$PWD/../rssfeed-common.pri)
include($$PWD/rssfeed-posts.pri)

rssfeed_posts_sync_profile.path = /etc/buteo/profiles/sync
rssfeed_posts_sync_profile.files = $$PWD/rssfeed.Posts.xml
rssfeed_posts_client_plugin_xml.path = /etc/buteo/profiles/client
rssfeed_posts_client_plugin_xml.files = $$PWD/rssfeed-posts.xml

HEADERS += rssfeedpostsplugin.h
SOURCES += rssfeedpostsplugin.cpp

OTHER_FILES += \
    rssfeed_posts_sync_profile.files \
    rssfeed_posts_client_plugin_xml.files

INSTALLS += \
    target \
    rssfeed_posts_sync_profile \
    rssfeed_posts_client_plugin_xml
