#!/bin/sh

if [ ReplayApplet.class -ot ReplayApplet.java ]
then
	javac ReplayApplet.java || exit 1
fi
appletviewer -J-Djava.security.policy=policy index.html
