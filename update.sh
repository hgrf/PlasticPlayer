#!/bin/bash
BASE_URL=http://10.42.0.1:9000
set -e

echo "Uploading update..."
curl --progress-bar -X POST $BASE_URL/firmware/upload \
    -H "Content-Type: multipart/form-data" \
    -F "file=@output/images/update.raucb" \
    > /dev/null

echo "Installing update..."
curl -s --fail -X POST $BASE_URL/firmware/install \
    -H "Content-Type: application/json" \
    --data '{"filename":"update.raucb"}' \
    > /dev/null

set +e
while [ "$?" != "1" ]
do
  sleep 1
  curl -s -X GET $BASE_URL/firmware/install_progress \
    | python3 -c "import sys, json; prog, msg, _ = json.load(sys.stdin); print(f'\33[2K\r{prog}%\t{msg}', end=''); prog == 100 and sys.exit(1)"
done
set -e

echo -e "\nRebooting..."
curl -s -X POST $BASE_URL/reboot > /dev/null
