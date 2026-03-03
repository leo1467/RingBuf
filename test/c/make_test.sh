#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd ${DIR}/../../src
make clean && make BUILD=debug BUILD_TYPE=static install
cd ${DIR}
make
