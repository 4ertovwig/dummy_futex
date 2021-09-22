#!/bin/bash

PROJECT_NAME=dummy_futex

docker build . -t $PROJECT_NAME
docker run --rm $PROJECT_NAME