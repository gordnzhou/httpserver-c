#!/bin/sh

IMAGE_NAME="http_server"
CONTAINER_NAME="httpserver-c"
DIRECTORY="/httpserver-c"
HOST_PORT=8008 # keep host port same for now, might need to change if deployed
CONT_PORT=8008

do_build=0

for arg in "$@"; 
do
    if [ "$arg" == "build" ]; then
        do_build=1
    fi
done

if [ "$do_build" -eq 1 ]
then 
    echo "Building Docker image ${IMAGE_NAME}..."
    docker build -t "${IMAGE_NAME}" .
    echo "Build complete."
else 
    # ROOT_DIR="$(pwd)"
    # -v "${ROOT_DIR}/":/httpserver-c
    echo "Running container ${CONTAINER_NAME} from image ${IMAGE_NAME}..."
    docker run --rm -it --name="${CONTAINER_NAME}" -p ${HOST_PORT}:${CONT_PORT} "${IMAGE_NAME}"
fi