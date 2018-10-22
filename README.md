# osm-ogr-tools

## osm2ogr_with_tags

    osm2ogr_with_tags (version 0.1.0)

    Convert OSM data to OGR formats. This tools allows to export arbitary tags to
    OGR fields.

    Options:
      -h [ --help ]            Print help messages
      -v [ --version ]         Print version and exit
      --length                 Add a field containing the length of features. The
                               units are meters. The name of the field will be
                               "osm_length". This option only applies when ways are
                               expored.
      --is_closed              Add a field describing if a way is closed.This
                               option only applies when ways are expored.
      -t [ --tag ] arg         Tags to create columns for. This option may be used
                               multiple times to add more than one tag.
      -w [ --ways ]            Convert ways instead of nodes. Default is nodes.
      -f [ --format_name ] arg Outputformat. Default is "ESRI Shapefile"
      -o [ --outputfile ] arg  Name of the output file
      -i [ --inputfile ] arg   Name of the input file


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
