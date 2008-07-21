#!/bin/bash

for i in $(find . -regex "\./..\(_..\)?\.po"); do
	./update_po_from_wiki.sh ${i%.*}
done

