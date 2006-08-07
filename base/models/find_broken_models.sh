#!/bin/sh

find . | xargs strings -f | grep crattack
