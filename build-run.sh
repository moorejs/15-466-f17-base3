# usage: add any parameter after ./build-run.sh to just build instead of building and running

build="jam -sDEBUG=true"

function build_client {
  printf "\nBuilding client...\n"
  cd client
  $build
  success=$?
  cd ..
}

function build_server {
  printf "\nBuilding server...\n"

  cd server
  $build
  success=$?
  cd ..
}

if build_client && [ $success -eq 0 ] && build_server && [ $success -eq 0 ] ; then
  printf "\nBuilt successfully\n\n"
  
  if [ "$1" == "menu" ] ; then
    cd dist
    ./client menu
  elif [ -z ${1+x} ] ; then # if $1 is not set
    printf "Starting two clients (one cop, one robber), and a server...\n\n"

    cd dist
    ./client localhost 9001 300 staging cop | sed "s/^/[cop] /" &
    ./client localhost 9001 300 staging robber | sed "s/^/[robber] /" &
    ./server 9001 | sed "s/^/[server] /"
  else
    exit 0
  fi
fi
