#!/bin/bash

pushd osm_extract_diagram
dot -o../osm_extract_diagram.png -Tpng osm_extract_diagram.dot
popd
