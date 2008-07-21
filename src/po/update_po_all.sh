#!/bin/bash

for i in $(find . -regex "\./..\(_..\)?\.po"); do
	#strip extension
	j=${i%.*}
	./update_po_from_wiki.sh $(basename $j)
done

