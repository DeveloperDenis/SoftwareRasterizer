#!/bin/bash

APP_NAME=GraphicsTesting

INCLUDES=-I../developerdenis/
DISABLED_WARNINGS="-Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable"

CFILES="../code/main.cpp"

pushd ../build/ > /dev/null

g++ $CFILES -DDEBUG -DDENIS_LINUX $INCLUDES -Werror -Wall $DISABLED_WARNINGS -o $APP_NAME -lX11 -lrt

popd > /dev/null
