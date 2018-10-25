/*
 * Copyright 2018 Deutsches Zentrum für Luft- und Raumfahrt e.V.
 *         (German Aerospace Center), German Remote Sensing Data Center
 *         Department: Geo-Risks and Civil Security
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "boost/program_options.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <functional>
#include <iterator>

#include "version.hpp"

#include <gdalcpp.hpp>

#include <osmium/index/map/all.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/visitor.hpp>

#include <osmium/geom/ogr.hpp>
#include <osmium/geom/haversine.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/util/progress_bar.hpp>


#define PROGRAM_NAME "osm2ogr_with_tags"
#define LENGTH_FIELD_NAME "osm_length"
#define DEFAULT_OUTPUT_FORMAT "ESRI Shapefile"
#define DEFAULT_LAYER_NAME "export"


namespace
{
    const size_t ERROR_WRONG_ARGUMENTS = 1;
    const size_t SUCCESS = 0;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;
}

using index_type = osmium::index::map::Map<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

typedef std::function<void(void)> progress_callback_t;


inline double haversine_distance(const osmium::NodeRef* start, const osmium::NodeRef* end) {
    double sum_length = 0;
    for (auto it = start; it != end; ++it) {
        if (std::next(it) != end) {
            sum_length += osmium::geom::haversine::distance(it->location(), std::next(it)->location());
        }
    }
    return sum_length;
}

class GenericOGRHandler : public osmium::handler::Handler {

protected:

    osmium::geom::OGRFactory<> ogr_factory;

    std::set<std::string> tags;

    progress_callback_t progress_cb;
    int progress_ticks = 0;

    void setTagsOfFeature(gdalcpp::Feature &feature, const osmium::OSMObject &osmobj) {
        for(const std::string& tag : this->tags) {
            feature.set_field(tag.c_str(), osmobj.tags().get_value_by_key(tag.c_str()));
        }
    }

    void addTagFieldsToLayer(gdalcpp::Layer &layer) {
        for(const std::string& tag : this->tags) {
            layer.add_field(tag, OFTString, 200);
        }
    }

    void addDefaultFieldsToLayer(gdalcpp::Layer &layer) {
        layer.add_field("osm_id", OFTReal, 10);
    }

    void setDefaultFieldsOfFeature(gdalcpp::Feature &feature, const osmium::OSMObject &osmobj) {
        feature.set_field("osm_id", static_cast<double>(osmobj.id()));
    }

    void setExportTags(std::vector<std::string> &tags) {
        this->tags.erase(this->tags.begin(), this->tags.end());
        std::copy(tags.begin(), tags.end(), std::inserter(this->tags, this->tags.end()));
    }

    void updateProgress() {
        if (progress_cb) {
            // do not update the progress for each call, as this will become expensive
            if (progress_ticks > 200) {
                progress_cb();
                progress_ticks = 0;
            }
            progress_ticks++;
        }
    }

public:

    void setProgressCallback(progress_callback_t progress_cb) {
        this->progress_cb = progress_cb;
    }
};


class NodeOGRHandler : public GenericOGRHandler {

protected:
    gdalcpp::Layer layer;

public:

    explicit NodeOGRHandler(gdalcpp::Dataset& dataset, std::string& layerName, std::vector<std::string> &tags) :
        layer(dataset, layerName, wkbPoint) {

        setExportTags(tags);
        addDefaultFieldsToLayer(layer);
        addTagFieldsToLayer(layer);

    }

    void node(const osmium::Node& node) {
        try {
            gdalcpp::Feature feature{layer, ogr_factory.create_point(node)};
            setDefaultFieldsOfFeature(feature, node);
            setTagsOfFeature(feature, node);
            feature.add_to_layer();
        }
        catch (osmium::invalid_location& e) {
            std::cerr << "Ignoring node with an invalid location" << std::endl;
        }
        updateProgress();
    }
};



class WayOGRHandler : public GenericOGRHandler {

protected:
    gdalcpp::Layer layer;
    bool includeLength;

    const char * wayPartFieldName = "way_part";

public:

    explicit WayOGRHandler(gdalcpp::Dataset& dataset, std::string& layerName, std::vector<std::string> &tags, bool includeLength) :
        layer(dataset, layerName, wkbLineString),
        includeLength(includeLength) {

        setExportTags(tags);
        addDefaultFieldsToLayer(layer);
        layer.add_field(wayPartFieldName, OFTReal, 10);
        if (includeLength) {
            layer.add_field(LENGTH_FIELD_NAME, OFTReal, 12, 3);
        }
        addTagFieldsToLayer(layer);

    }

    void way(const osmium::Way& way) {

        // extractions using osmium may leave some invalid locations in ways. So we remove
        // these parts of the ways and just return the valid parts as seperate features
        // See // https://osmcode.org/osmium-tool/manual.html#creating-geographic-extracts :
        // "... but they might be missing some nodes, they are not reference-complete... "
        const osmium::WayNodeList& nodes = way.nodes();
        auto node_it = nodes.begin();
        int part = 0;
        while(node_it != nodes.end()) {
            if (node_it->location().valid()) {
                auto part_start = node_it;
                while (true) {
                    auto next_node = std::next(node_it);
                    if ((!next_node->location().valid()) || (next_node == nodes.end())) {
                        break;
                    }
                    node_it++;
                }
                if (part_start != node_it) {
                    ogr_factory.linestring_start();
                    size_t num_points = ogr_factory.fill_linestring_unique(part_start, std::next(node_it));

                    gdalcpp::Feature feature{layer, ogr_factory.linestring_finish(num_points)};

                    setDefaultFieldsOfFeature(feature, way);
                    feature.set_field(wayPartFieldName, static_cast<double>(part++));
                    if (includeLength) {
                        feature.set_field(LENGTH_FIELD_NAME, haversine_distance(part_start, std::next(node_it)));
                    }
                    setTagsOfFeature(feature, way);
                    feature.add_to_layer();
                }

            }
            node_it++;
        }

        updateProgress();
    }
};



int main(int argc, char* argv[]) {

    try {
        std::vector<std::string> tags;
        std::string outputfile_name;
        std::string inputfile_name;
        std::string outputfile_format = DEFAULT_OUTPUT_FORMAT;
        std::string layerName = DEFAULT_LAYER_NAME;
        bool convertWays = false;
        bool includeLength = false;
        bool useProgressBar = false;

        namespace po = boost::program_options;
        po::options_description options("Options");

        po::options_description main_options("Input/Output");
        main_options.add_options()
            ("inputfile,i", po::value<std::string>(&inputfile_name)->required(), "Name of the OSM input file")
            ("outputfile,o", po::value<std::string>(&outputfile_name)->required(), "Name of the OSM output file")
            ("format_name,f", po::value<std::string>(&outputfile_format), "Outputformat. "
                        "For a list of supported formats see the output of the \"ogrinfo --formats\" command. "
                        "The default is \"" DEFAULT_OUTPUT_FORMAT "\".")
            ("layer_name,l", po::value<std::string>(&layerName), "Layer name of the exported layer. "
                        "The default is \"" DEFAULT_LAYER_NAME "\".")
            ("tag,t", po::value<std::vector<std::string> >(&tags), "Tags to create columns for. This option "
                        "may be repeated multiple times to add more than one tag.")
            ("ways,w", "Convert ways instead of nodes. Default is nodes.")
            ("length", "Add a field containing the length of features. "
                        "The name of the field will be \"" LENGTH_FIELD_NAME "\". "
                        "This option only applies when ways are exported. "
                        "The units are meters.");
        options.add(main_options);

        po::options_description general_options("General");
        general_options.add_options()
            ("help,h", "Print help message.")
            ("version,v", "Print version and exit.")
            ("progress,p", "Display a progress bar showing the percentage of the inputfile "
                        "which has been processed. As PBF files are sorted by type, the "
                        "output can be a bit misleading, but gives a general idea of the "
                        "progress made.");
        options.add(general_options);

        po::positional_options_description positionalOptions;
        positionalOptions.add("outputfile", -1);
        positionalOptions.add("inputfile", -1);

        po::variables_map vm;

        try {
            po::store(po::command_line_parser(argc, argv)
                            .options(options)
                            .positional(positionalOptions)
                            .run(),
                    vm); // can throw

            if ( vm.count("help") ) {
                std::cout
                    << PROGRAM_NAME << " (version " << OSM2OGR_VERSION_FULL << ")"
                    << std::endl
                    << std::endl
                    << "Convert OSM data to OGR formats. This tools allows to export arbitary tags to"
                    << std::endl
                    << "OGR fields."
                    << std::endl
                    << std::endl
                    << options
                    << std::endl;
                return SUCCESS;
            }

            if ( vm.count("version") ) {
                std::cout
                    << OSM2OGR_VERSION_FULL
                    << std::endl;
                return SUCCESS;
            }

            convertWays =  vm.count("ways") != 0;
            includeLength =  vm.count("length") != 0;
            useProgressBar = vm.count("progress") != 0;

            po::notify(vm); // throws on error, so do after help in case
                              // there are any problems
        }
        catch(po::error& e) {
            std::cerr
                << "ERROR: "
                << e.what() << std::endl << std::endl
                << options
                << std::endl;
            return ERROR_WRONG_ARGUMENTS;
        }

        // app code here //
        {
            const auto& map_factory = osmium::index::MapFactory<osmium::unsigned_object_id_type, osmium::Location>::instance();

            osmium::io::Reader reader{inputfile_name};

            std::string location_store{"flex_mem"};
            std::unique_ptr<index_type> index = map_factory.create_map(location_store);
            location_handler_type location_handler{*index};
            location_handler.ignore_errors();

            CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");
            CPLSetConfigOption("SHAPE_ENCODING", "UTF8");
            gdalcpp::Dataset dataset{outputfile_format, outputfile_name, gdalcpp::SRS{}, {}};

            auto progressbar = osmium::ProgressBar(reader.file_size(), useProgressBar);
            progress_callback_t progress_cb = [&progressbar, &reader]() {
                progressbar.update(reader.offset());
            };
            progress_cb();

            if (convertWays) {
                auto handler = WayOGRHandler(dataset, layerName, tags, includeLength);
                handler.setProgressCallback(progress_cb);
                osmium::apply(reader, location_handler, handler);
            }
            else {
                auto handler = NodeOGRHandler(dataset, layerName, tags);
                handler.setProgressCallback(progress_cb);
                osmium::apply(reader, location_handler, handler);
            }
            progressbar.done();
            reader.close();
        }
    }
    catch(std::exception& e) {
        std::cerr
            << "Unhandled Exception: "
            << e.what()
            << ", application will now exit"
            << std::endl;
        return ERROR_UNHANDLED_EXCEPTION;
  }

  return SUCCESS;
}
