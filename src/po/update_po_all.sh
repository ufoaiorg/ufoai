#!/bin/bash

for i in *.po; do
    ./update_po_from_wiki.sh $i
done
