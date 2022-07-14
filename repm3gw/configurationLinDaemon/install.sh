#!/bin/bash
# Script for preparing installation from pre-built components

PDIR_PCK="."
mkdir -p "$PDIR_PCK"

PDIR_USRS="/usr/share/repm3gw"
PDIR_VAR="/var/cache/repm3gw"
PDIR_ETC="/etc/repm3gw"
PDIR_USRL="/usr/lib/repm3gw"
PDIR_USRB="/usr/bin"
PDIR_SYSD="/lib/systemd/system"
PDIR_OPT="/opt"

# create target install directories
mkdir -p "$PDIR_USRS"
mkdir -p "$PDIR_VAR"
mkdir -p "$PDIR_ETC"
mkdir -p "$PDIR_USRL"

# stop service
systemctl stop repm3gw.service

# copy unit file and enable service
cp "$PDIR_PCK$PDIR_SYSD/repm3gw.service" "$PDIR_SYSD/"
systemctl enable repm3gw.service

# copy components
cp "$PDIR_PCK$PDIR_USRL/"* "$PDIR_USRL/"

# copy repm3gw binary
cp "$PDIR_PCK$PDIR_USRB/repm3gw" "$PDIR_USRB/"

# copy configurations
cp "$PDIR_PCK$PDIR_ETC/"* "$PDIR_ETC/"

# copy database scripts
mkdir -p "$PDIR_USRS/DataBase/"
cp -r "$PDIR_PCK$PDIR_USRS/DataBase/"* "$PDIR_USRS/DataBase/"

# copy bootstrap certificates
mkdir -p "$PDIR_USRS/cert/"
cp -r "$PDIR_PCK$PDIR_USRS/cert/"* "$PDIR_USRS/cert/"

# start service
systemctl start repm3gw.service
