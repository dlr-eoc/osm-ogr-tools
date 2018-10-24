# osm-ogr-tools

## osm2ogr_with_tags

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
            libboost-program-options-dev


Build:


    cd osm-ogr-tools
    mkdir build
    cd build
    cmake ..
    make
