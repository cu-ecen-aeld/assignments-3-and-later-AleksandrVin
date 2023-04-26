#! /bin/sh

NAME=aesdsocket

case "$1" in
  start)
    echo "Starting aesdsocket"
    start-stop-daemon --start -n $NAME --exec /usr/bin/$NAME -- -d
    ;;
  stop)
    echo "Stopping aesdsocket"
    start-stop-daemon --stop -n $NAME
    ;;
  *)
    echo "Usage: $0 {start|stop}"
    exit 1
    ;;
esac

exit 0