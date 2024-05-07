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
    GIT_CMD="git log --diff-filter=r --format=format:%ad:%s --date=format-local:%Y"
    # ... "--diff-filter=r" to skip move/rename commits
    # ... "--format" to print "YEAR:SUBJECT"
    if [[ "$FILE" != "./LICENSE.txt" ]]; then
        # last change of the file
        GIT_CMD="$GIT_CMD --follow $FILE"
        # ... else (LICENSE.txt) last change of the repository
    fi
    GIT_YEAR=$(TZ=UTC $GIT_CMD | grep -v '[0-9]\{4\}:Update copyright years' | head -n 1 | cut -d : -f 1 || true)
    if [[ "$COPYRIGHT_YEAR" != "${GIT_YEAR}" ]]; then
        echo "COPYRIGHT_YEAR ($COPYRIGHT_YEAR) != GIT_YEAR ($GIT_YEAR) in $FILE"
    fi
done
