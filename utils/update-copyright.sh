#!/bin/bash

find . -type f -exec grep -Il "Copyright" {} \;     \
| grep -v \.git \
| while read file;                            \
do \
  LC_ALL=C sed -b -i "s/\(.*Copyright.*\)[0-9]\{4\}\( *Vitaly Novichkov\)/\1`date +%Y`\2/" "$file"; \
done
