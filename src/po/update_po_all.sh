#!/bin/bash

for i in $(ls *.po); do
./update_po_from_wiki.sh $i
done

