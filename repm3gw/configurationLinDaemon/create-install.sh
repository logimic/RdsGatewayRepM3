#!/bin/bash
# Script to prepare repm3gw installation from pre-built components

PDIR_PCK="./repm3gw-pckg/"
rm -rf "$PDIR_PCK*"
mkdir -p "$PDIR_PCK"

PDIR_USRS="/usr/share/repm3gw/"
PDIR_VAR="/var/cache/repm3gw/"
PDIR_ETC="/etc/repm3gw/"
PDIR_USRL="/usr/lib/repm3gw/"
PDIR_USRB="/usr/bin/"
PDIR_SYSD="/lib/systemd/system/"
PDIR_OPT="/opt/"

# create pckg install directories
mkdir -p "$PDIR_PCK/$PDIR_USRS"
mkdir -p "$PDIR_PCK/$PDIR_VAR"
mkdir -p "$PDIR_PCK/$PDIR_ETC"
mkdir -p "$PDIR_PCK/$PDIR_USRL"
mkdir -p "$PDIR_PCK/$PDIR_USRB"
mkdir -p "$PDIR_PCK/$PDIR_SYSD"
mkdir -p "$PDIR_PCK/$PDIR_OPT"

# copy unit service file
cp ./repm3gw.service "$PDIR_PCK/$PDIR_SYSD"

# copy shape components
cp ../../../shape/bin/libTraceFileService.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../shape/bin/libTraceFormatService.so "$PDIR_PCK/$PDIR_USRL"

# copy shapeware components
cp ../../../shapeware/bin/libWebsocketCppClientService.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../shapeware/bin/libMqttService.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../shapeware/bin/libSyslogLogger.so "$PDIR_PCK/$PDIR_USRL"

# copy oegw components
cp ../../../oegw/bin/libBackupRestore.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libIdentityProvider.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libIpAdrProvider.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libTimePointProvider.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libIqrfCloudApiHandler.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libMsgMqttProvisioning.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libWorkerThread.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libServiceTopicListener.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libIqrfAdapt2.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../oegw/bin/libAwsFleetProv.so "$PDIR_PCK/$PDIR_USRL"

# copy repm3 components
cp ../../../RdsGatewayRepM3/bin/libRepM3Edge.so "$PDIR_PCK/$PDIR_USRL"
cp ../../../RdsGatewayRepM3/bin/repm3gw "$PDIR_PCK/$PDIR_USRB"

# copy configurations
cp ./configuration/*.json "$PDIR_PCK/$PDIR_ETC"

# copy config file
cp ./config.json "$PDIR_PCK/$PDIR_ETC"

# copy bootstrap certificates
mkdir -p "$PDIR_PCK/$PDIR_USRS/cert/"
cp -r ./cert/* "$PDIR_PCK/$PDIR_USRS/cert/"

# copy backup dir
cp -r ./configuration/backup "$PDIR_PCK/$PDIR_USRS/"

# copy install scripts
cp ./purge.sh "$PDIR_PCK/"
cp ./install.sh "$PDIR_PCK/"

cp ./update-install.sh "$PDIR_PCK/"

# copy tool scripts
mkdir -p "$PDIR_PCK/scripts/"
cp -r ./scripts/* "$PDIR_PCK/scripts/"

# compress
zip -r repm3gw-pckg.zip "$PDIR_PCK"

