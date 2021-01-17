#!/bin/bash
set -e

if [ "${1#-}" != "$1" ]; then
	set -- ebusd "$@"
fi

exec "$@"