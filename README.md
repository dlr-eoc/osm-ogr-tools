# osm-ogr-tools

## osm2ogr_with_tags

    osm2ogr_with_tags (version 0.1.0)

    Convert OSM data to OGR formats. This tools allows to export arbitary tags to
    OGR fields.

    Options:
      -h [ --help ]            Print help messages
      -v [ --version ]         Print version and exit
      --length                 Add a field containing the length of features. The 
                               name of the field will be "osm_length". This option 
                               only applies when ways are exported. The units are 
                               meters.
      -t [ --tag ] arg         Tags to create columns for. This option may be used 
                               multiple times to add more than one tag.
      -p [ --progress ]        Display a progress bar shwoing the percentage of the
                               inputfile which has been processed. As PBF files are
                               sorted by type, the output can be a bit misleading, 
                               but gives a general idea of the progress made.
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
