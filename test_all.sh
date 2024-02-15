#!/bin/sh
(cd src/lib/ebus/test && make >/dev/null && ./test_filereader && ./test_data && ./test_message && ./test_symbol && echo "standard: OK!")|grep -Ev "OK$"
(cd src/lib/ebus/contrib/test && make >/dev/null && ./test_contrib && echo "contrib: OK!")|grep -Ev "OK$"
