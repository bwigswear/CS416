#!/bin/bash
fusermount -u /tmp/kpl56/mountdir
echo "If something prints below, it was not unmounted"
findmnt | grep kpl56
git pull
make clean
make
rm DISKFILE
./rufs -d -s /tmp/kpl56/mountdir
