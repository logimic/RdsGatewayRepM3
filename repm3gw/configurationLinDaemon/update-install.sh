#!/usr/bin/env bash
# Script to download asset file from tag release using GitHub API v3.
# See: http://stackoverflow.com/a/35688093/55075    
CWD="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"

# Check dependencies.
set -e
type curl grep sed tr unzip >&2
xargs=$(which gxargs || which xargs)

# access_token can be stored securely by: https://specifications.freedesktop.org/secret-service/latest/index.html
# not used now
# [ -f ~/.secrets ] && source ~/.secrets 
GITHUB_API_TOKEN=e17ae30626a128b041b4b051bf877b97aeb9a82e

# Define variables.
owner=logimic
repo=RdsGatewayRepM3
name=repm3gw-pckg.zip
nameunzip=repm3gw-pckg
GH_API="https://api.github.com"
GH_REPO="$GH_API/repos/$owner/$repo"
AUTH="Authorization: token $GITHUB_API_TOKEN"
WGET_ARGS="--content-disposition --auth-no-challenge --no-cookie"
CURL_ARGS="-LJO#"

# Validate settings.
[ "$GITHUB_API_TOKEN" ] || { echo "Error: Please define GITHUB_API_TOKEN variable." >&2; exit 1; }
[ $# -gt 1 ] && { echo "Usage: $0 <tag>"; exit 1; }
if [ $# -eq 1 ]
then  
  read tag <<<$@
  echo "Requested to installing version: $tag"
  GH_TAGS="$GH_REPO/releases/tags/$tag"
else
  echo "Requested to install LATEST"
  GH_TAGS="$GH_REPO/releases/latest"
fi 

# Validate token.
curl -o /dev/null -sH "$AUTH" $GH_REPO || { echo "Error: Invalid repo, token or network issue!";  exit 1; }

# Read asset tags.
response=$(curl -sH "$AUTH" $GH_TAGS)
# Get ID of the asset based on given name.
eval $(echo "$response" | grep -C3 "name.:.\+$name" | grep -w id | tr : = | tr -cd '[[:alnum:]]=')
#id=$(echo "$response" | jq --arg name "$name" '.assets[] | select(.name == $name).id') # If jq is installed, this can be used instead. 
[ "$id" ] || { echo "Error: Failed to get asset id, response: $response" | awk 'length($0)<100' >&2; exit 1; }
GH_ASSET="$GH_REPO/releases/assets/$id"

echo "Removing old install package" >&2
rm -rf $nameunzip $name

# Download asset file.
echo "Downloading asset... " >&2
curl $CURL_ARGS -H 'Accept: application/octet-stream' "$GH_ASSET?access_token=$GITHUB_API_TOKEN"

echo "Removing old install package"
[ -f ./$name ] || { echo "Cannot download $name." >&2; exit 1; }

unzip $name

# interact to get uid/gid
read -p "Ready to install continue? [Y/N]: "  confirm
[ "$confirm" = "Y" ] || { echo "Installation is canceled." >&2; exit 1; }

pushd ./repm3gw-pckg

echo "Purging old installation" >&2
./purge.sh

echo "Installing ... " >&2
./install.sh

echo "Customizing ... " >&2
./customize.sh

popd

echo "$0 done." >&2
