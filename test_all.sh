#!/bin/sh
(cd src/lib/ebus/test && make >/dev/null && ./test_filereader && ./test_data && ./test_message && ./test_symbol && echo "standard: OK!")|egrep -v "OK$"
(cd src/lib/ebus/contrib/test && make >/dev/null && ./test_contrib && echo "contrib: OK!")|egrep -v "OK$"
(cd src/lib/knx/test && make >/dev/null && ./test_knxhandler && echo "knx: OK!")|egrep -v "OK$"
