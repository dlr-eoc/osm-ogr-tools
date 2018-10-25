#!/usr/bin/env python3
#
#
# Dependencies:
#
#   commandlinte tools from:
#       - https://github.com/osmcode/osmium-tool (ubuntu package: osmium-tool)
#       - http://git.ukis.eoc.dlr.de/users/mand_nc/repos/osm-ogr-tools
#

from osgeo import ogr, osr

import sys
import json
import subprocess
import argparse
import tempfile
import contextlib
import shutil
import os

def get_multipolygon_from_file(filename):
    """condense teh geometries from the filter file into one multipolygon"""

    ds = ogr.Open(filename)
    layer = ds.GetLayer(0)

    target_spatialref = osr.SpatialReference()
    if target_spatialref.ImportFromEPSG(4326) != 0:
        raise IOError("Could not import EPSG:4326 spatial reference")

    aggregation = ogr.Geometry(ogr.wkbMultiPolygon)

    def add_to_aggregation(polygon_geom):
        nonlocal aggregation, target_spatialref

        # transform to OSMs coordinate system
        sr = polygon_geom.GetSpatialReference()
        if sr and not target_spatialref.IsSame(target_spatialref):
            polygon_geom.TransformTo(target_spatialref)

        aggregation = aggregation.Union(polygon_geom)

    for feature in layer:
        geom = feature.GetGeometryRef()
        if geom.GetGeometryType() == ogr.wkbPolygon:
            add_to_aggregation(geom)
        elif geom.GetGeometryType() == ogr.wkbMultiPolygon:
            for i in range(0, geom.GetGeometryCount()):
                add_to_aggregation(geom.GetGeometryRef(i))
        else:
            raise IOError("Unsupported geometry type: " + geom.GetGeometryName())

    if aggregation.IsEmpty():
        return None
    return aggregation


def get_osmium_tool_config(output_filename=None, directory=None, spatial_filter_file=None):
    extract_cfg = {
        'output': output_filename,
        'output_format': 'pbf',
        'description': 'auto-extracted subset'
    }

    if spatial_filter_file:
        filter_multipolygon = get_multipolygon_from_file(spatial_filter_file)
        if filter_multipolygon:
            json_geom = json.loads(filter_multipolygon.ExportToJson())
            extract_cfg['multipolygon'] = json_geom['coordinates']

    cfg = {
        'directory': directory,
        'extracts': [ extract_cfg, ]
    }
    return cfg


@contextlib.contextmanager
def tempdir():
    tmpdir = tempfile.mkdtemp(prefix='osm-extract')
    try:
        yield tmpdir
    finally:
        try:
            shutil.rmtree(tmpdir)
        except:
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description="Extract geographical subsets from an OSM-PBF file and export the data to a GIS format.",
            formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('--geofilter', help="Vector-dataset to use as a spatial filter. "
                    + "Geometry type must be polygon or multipolygon.")
    parser.add_argument('-f', '--format_name', default='ESRI Shapefile', help='Outputformat. '
                    + 'For a list of supported formats see the output of the "ogrinfo --formats" command. '
                    + 'The default is "ESRI Shapefile".')
    parser.add_argument('-l', '--layer_name', default='export', help='Layer name of the exported layer.')
    parser.add_argument('--length', action='store_true', help='Add a field containing the length of features. '
                    + 'This option only applies when ways are exported. The units are meters.')
    parser.add_argument('-w', '--ways', action='store_true', help='Convert ways instead of nodes. Default is nodes.')
    parser.add_argument('-t', '--tags', nargs='*', help='Tags to create columns for.')
    parser.add_argument('-s', '--strategy', default='complete_ways',
                    help='Strategy to create geographical extracts. See the "osmium extract" manual for more on this topic.')
    parser.add_argument('osm_input_file', help="OSM input file")
    parser.add_argument('ogr_output_file', help="OGR output file")
    args = parser.parse_args()

    former_workdir = os.getcwd()
    with tempdir() as tdir:
        os.chdir(tdir)
        osmium_cfg_name = 'cfg.json'
        input_file = os.path.join(former_workdir, args.osm_input_file)

        # filter using the geofilter - if provided
        if args.geofilter:
            input_file = 'o.pbf'
            with open(osmium_cfg_name, 'w') as fh:
                fh.write(json.dumps(get_osmium_tool_config(
                            output_filename=input_file,
                            directory=tdir,
                            spatial_filter_file=os.path.join(former_workdir, args.geofilter)
                )))
                fh.flush()
                print("\nExtracting region from the input data")
                subprocess.run([
                            'osmium',
                            'extract',
                            '-c', osmium_cfg_name, os.path.join(former_workdir, args.osm_input_file),
                            '-s', args.strategy
                        ],
                        check=True
                )

        print("\nConverting to vector format")
        convertion_args = [
            'osm2ogr_with_tags',
            '-i', input_file,
            '-o', os.path.join(former_workdir, args.ogr_output_file),
            '-p'
        ]
        if args.ways:
            convertion_args.append('--ways')
        if args.length:
            convertion_args.append('--length')
        if args.format_name:
            convertion_args.append('--format_name')
            convertion_args.append(args.format_name)
        if args.layer_name:
            convertion_args.append('--layer_name')
            convertion_args.append(args.layer_name)
        if args.tags:
            for t in args.tags:
                convertion_args.append('--tag')
                convertion_args.append(t)
        subprocess.run(
                convertion_args,
                check=True
        )

    os.chdir(former_workdir)

