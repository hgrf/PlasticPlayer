#!/bin/sh

netcat 127.0.0.1 42070 <<EOF
PLAYER_EVENT=$PLAYER_EVENT
ALBUM=$ALBUM
ARTISTS=$ARTISTS
NAME=$NAME
EOF
