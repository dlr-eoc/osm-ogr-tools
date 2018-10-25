# osm-ogr-tools

Tools for working with OSM data. Most of these tools are based on [osmium-tool](https://github.com/osmcode/osmium-tool) and
the [libosmium library](https://github.com/osmcode/libosmium).

## Included tools and libraries

### osm_extract.py

Wrapper to combine `osm2ogr_with_tags` and `osmium`. This script can also be imported as a python module into
own programs. It provides the `osm_ogr_extract` function.


    usage: osm_extract.py [-h] [--geofilter GEOFILTER] [-f FORMAT_NAME]
                          [-l LAYER_NAME] [--length] [-w] [-t [TAGS [TAGS ...]]]
                          [-s STRATEGY]
                          osm_input_file ogr_output_file

    Extract geographical subsets from an OSM-PBF file and export the data to a GIS
    format.

    positional arguments:
      osm_input_file        OSM input file
      ogr_output_file       OGR output file

    optional arguments:
      -h, --help            show this help message and exit
      --geofilter GEOFILTER
                            Vector-dataset to use as a spatial filter. Geometry
                            type must be polygon or multipolygon. (default: None)
      -f FORMAT_NAME, --format_name FORMAT_NAME
                            Outputformat. For a list of supported formats see the
                            output of the "ogrinfo --formats" command. (default:
                            ESRI Shapefile)
      -l LAYER_NAME, --layer_name LAYER_NAME
                            Layer name of the exported layer. (default: export)
      --length              Add a field containing the length of features. This
                            option only applies when ways are exported. The units
                            are meters. (default: False)
      -w, --ways            Convert ways instead of nodes. Default is nodes.
                            (default: False)
      -t [TAGS [TAGS ...]], --tags [TAGS [TAGS ...]]
                            Tags to create columns for. (default: None)
      -s STRATEGY, --strategy STRATEGY
                            Strategy to create geographical extracts. See the
                            "osmium extract" manual for more on this topic.
                            (default: complete_ways)


### osm2ogr_with_tags

    osm2ogr_with_tags (version 0.1.0)

    Convert OSM data to OGR formats. This tools allows to export arbitary tags to
    OGR fields.

    Options:

    Input/Output:
      -i [ --inputfile ] arg    Name of the OSM input file
      -o [ --outputfile ] arg   Name of the OSM output file
      -f [ --format_name ] arg  Outputformat. For a list of supported formats see
                                the output of the "ogrinfo --formats" command. The
                                default is "ESRI Shapefile".
      -l [ --layer_name ] arg   Layer name of the exported layer. The default is
                                "export".
      -t [ --tag ] arg          Tags to create columns for. This option may be
                                repeated multiple times to add more than one tag.
      -w [ --ways ]             Convert ways instead of nodes. Default is nodes.
      --length                  Add a field containing the length of features. The
                                name of the field will be "osm_length". This option
                                only applies when ways are exported. The units are
                                meters.

    General:
      -h [ --help ]             Print help message.
      -v [ --version ]          Print version and exit.
      -p [ --progress ]         Display a progress bar showing the percentage of
                                the inputfile which has been processed. As PBF
                                files are sorted by type, the output can be a bit
                                misleading, but gives a general idea of the
                                progress made.


## Building this software

Install the dependencies (this command is ubuntu specific):

    apt-get install build-essential g++ libosmium2-dev cmake libgdal-dev \
            libboost-program-options-dev python3 python3-gdal osmium-tool


Build:

    cd osm-ogr-tools
    mkdir build
    cd build
    cmake ..
    make


# Legal an Licensing

This software is licensed under the [Apache 2.0 License](LICENSE.txt).

(c) 2018 German Aerospace Center (DLR); German Remote Sensing Data Center; Department: Geo-Risks and Civil Security

