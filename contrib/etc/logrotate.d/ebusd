# ebusd.log

/var/log/ebusd.log {
	rotate 5
	size=100k
	sharedscripts
	postrotate
	[ -f /var/run/ebusd.pid ] && /bin/kill -INT `cat /var/run/ebusd.pid`
	endscript
}
