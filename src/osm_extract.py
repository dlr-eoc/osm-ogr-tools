#!/usr/bin/env python3
#
#  Copyright 2018 Deutsches Zentrum für Luft- und Raumfahrt e.V.
#          (German Aerospace Center), German Remote Sensing Data Center
#          Department: Geo-Risks and Civil Security
#
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
# Dependencies:
#
#   commandline tools from:
#       - https://github.com/osmcode/osmium-tool (ubuntu package: osmium-tool)
#       - http://git.ukis.eoc.dlr.de/users/mand_nc/repos/osm-ogr-tools
#

from osgeo import ogr, osr

import sys
import json
import subprocess
import tempfile
import contextlib
import shutil
import os

def get_extraction_area_from_file(filename):
    """condense the geometries from the filter file into one geometry"""

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
        extraction_area = get_extraction_area_from_file(spatial_filter_file)
        if extraction_area:
            key = 'multipolygon'
            if extraction_area.GetGeometryType() == ogr.wkbPolygon:
                key = 'polygon'
            json_geom = json.loads(extraction_area.ExportToJson())
            extract_cfg[key] = json_geom['coordinates']

    cfg = {
        'directory': directory,
        'extracts': [ extract_cfg, ]
    }
    return cfg


@contextlib.contextmanager
def temporary_directory(temp_base=tempfile.gettempdir()):
    tmpdir = tempfile.mkdtemp(prefix='osm-extract', dir=temp_base)
    try:
        yield tmpdir
    finally:
        try:
            shutil.rmtree(tmpdir)
        except:
            pass

@contextlib.contextmanager
def cd(target_dir):
    """
    Change to a directory while entering a context, change back to the
    original working directroy when leaving the context.
    """
    cwd = os.getcwd()
    try:
        os.chdir(target_dir)
        yield
    finally:
        os.chdir(cwd)


def osm_ogr_extract(osm_input_file,
                ogr_output_file,
                geofilter=None,
                format_name='ESRI Shapefile',
                layer_name='export',
                length=False,
                ways=False,
                tags=[],
                strategy='complete_ways',
                tempdir=tempfile.gettempdir()
                ):
    """convert an OSM input file to an OGR format."""

    former_workdir = os.getcwd()
    with temporary_directory(temp_base=tempdir) as tdir:
        with cd(tdir):
            osmium_cfg_name = 'cfg.json'
            input_file = os.path.join(former_workdir, osm_input_file)

            # filter using the geofilter - if provided
            if geofilter:
                input_file = 'o.pbf'
                with open(osmium_cfg_name, 'w') as fh:
                    fh.write(json.dumps(get_osmium_tool_config(
                                output_filename=input_file,
                                directory=tdir,
                                spatial_filter_file=os.path.join(former_workdir, geofilter)
                    )))
                    fh.flush()
                    print("\nExtracting region from the input data")
                    subprocess.run([
                                'osmium',
                                'extract',
                                '-c', osmium_cfg_name, os.path.join(former_workdir, osm_input_file),
                                '-s', strategy
                            ],
                            check=True
                    )

            print("\nConverting to vector format")
            convertion_args = [
                'osm2ogr_with_tags',
                '-i', input_file,
                '-o', os.path.join(former_workdir, ogr_output_file),
                '-p'
            ]
            if ways:
                convertion_args.append('--ways')
            if length:
                convertion_args.append('--length')
            if format_name:
                convertion_args.append('--format_name')
                convertion_args.append(format_name)
            if layer_name:
                convertion_args.append('--layer_name')
                convertion_args.append(layer_name)
            if tags:
                for t in tags:
                    convertion_args.append('--tag')
                    convertion_args.append(t)
            subprocess.run(
                    convertion_args,
                    check=True
            )



if __name__ == '__main__':
    import inspect
    import argparse

    def get_parameter_default(param_name):
        extract_signature = inspect.signature(osm_ogr_extract)
        default = extract_signature.parameters[param_name].default
        if default == inspect.Parameter.empty:
            return None
        return default

    parser = argparse.ArgumentParser(
            description="Extract geographical subsets from an OSM-PBF file and export the data to a GIS format.",
            formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('--geofilter', help="Vector-dataset to use as a spatial filter. "
                    + "Geometry type must be polygon or multipolygon.")
    parser.add_argument('-f', '--format_name', default=get_parameter_default('format_name'), help='Outputformat. '
                    + 'For a list of supported formats see the output of the "ogrinfo --formats" command.')
    parser.add_argument('-l', '--layer_name', default=get_parameter_default('layer_name'), help='Layer name of '
                    + 'the exported layer.')
    parser.add_argument('--length', action='store_true', default=get_parameter_default('length'),
                    help='Add a field containing the length of features. '
                    + 'This option only applies when ways are exported. The units are meters.')
    parser.add_argument('-w', '--ways', action='store_true', default=get_parameter_default('ways'),
                    help='Convert ways instead of nodes. Default is nodes.')
    parser.add_argument('-t', '--tags', nargs='*', help='Tags to create columns for.')
    parser.add_argument('-s', '--strategy', default=get_parameter_default('strategy'),
                    help='Strategy to create geographical extracts. See the "osmium extract" manual for more on this topic.')
    parser.add_argument('osm_input_file', help="OSM input file")
    parser.add_argument('ogr_output_file', help="OGR output file")
    args = parser.parse_args()

    osm_ogr_extract(args.osm_input_file,
                args.ogr_output_file,
                geofilter=args.geofilter,
                format_name=args.format_name,
                layer_name=args.layer_name,
                length=args.length,
                ways=args.ways,
                tags=args.tags,
                strategy=args.strategy
    )
