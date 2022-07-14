#!/bin/bash
# Script to purge repm3gw installation
# invoke with sudo

PDIR_USRS="/usr/share/repm3gw"
PDIR_VAR="/var/cache/repm3gw"
PDIR_ETC="/etc/repm3gw"
PDIR_USRL="/usr/lib/repm3gw"
PDIR_USRB="/usr/bin"
PDIR_SYSD="/lib/systemd/system"

# stop and disable service
systemctl stop repm3gw.service
systemctl disable repm3gw.service

# remove sql but keep original DB
rm -rf "$PDIR_USRS/DataBase/sql"

# remove cache
rm -rf "$PDIR_VAR"

# remove cfg
rm -rf "$PDIR_ETC"

# remove components
rm -rf "$PDIR_USRL"

# remove binary
rm "$PDIR_USRB/repm3gw"

# remove unit service file
rm "$PDIR_SYSD/repm3gw.service"

# keep logs


echo "repm3gw installation purge done"

