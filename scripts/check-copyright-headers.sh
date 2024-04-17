#!/bin/bash

set -euo pipefail

cd ${0%/*}/..

for FILE in $(find . -type f); do
    COPYRIGHT="$(head -n 1 $FILE)"
    if ! [[ "${COPYRIGHT,,}" =~ ^.*copyright.*$ ]]; then
        #echo "Skipping $FILE ($COPYRIGHT)"
        continue
    fi

    if ! [[ "$COPYRIGHT" =~ ^.*'Copyright © '([0-9]{4}(-([0-9]{4}))?).*', Prosoft Engineering, Inc. (A.K.A "Prosoft")'$ ]]; then
        echo "Invalid $FILE: $COPYRIGHT"
        continue
    fi
    COPYRIGHT_YEAR=${BASH_REMATCH[3]}
    if [[ ! "$COPYRIGHT_YEAR" ]]; then
        COPYRIGHT_YEAR=${BASH_REMATCH[1]}
    fi
    # git command to skip move/rename commit
    GIT_YEAR=$(TZ=UTC git log -n 1 --follow --diff-filter=r --format=format:%ad --date=format-local:%Y "$FILE")
    if [[ "$COPYRIGHT_YEAR" != "${GIT_YEAR}" ]]; then
        echo "COPYRIGHT_YEAR ($COPYRIGHT_YEAR) != GIT_YEAR ($GIT_YEAR) in $FILE"
    fi
done
