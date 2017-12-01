build=(jam -sDEBUG=true)

function build_client {
  cd client
  $build
  success=$?
  cd ..
}

function build_server {
  cd server
  $build
  success=$?
  cd ..
}

if build_client && [ $success -eq 0 ] && build_server && [ $success -eq 0 ] ; then
  cd dist
  ./client staging cop | sed "s/^/[cop] /" &
  ./client staging robber | sed "s/^/[robber] /" &
  ./server  | sed "s/^/[server] /"
fi
