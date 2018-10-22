#include "boost/program_options.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <set>

#include "version.hpp"

#include <gdalcpp.hpp>

#include <osmium/index/map/all.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/visitor.hpp>

#include <osmium/geom/ogr.hpp>
#include <osmium/geom/haversine.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>


#define PROGRAM_NAME "osm2ogr_tagged"
#define LENGTH_FIELD_NAME "osm_length"


namespace
{
  const size_t ERROR_WRONG_ARGUMENTS = 1;
  const size_t SUCCESS = 0;
  const size_t ERROR_UNHANDLED_EXCEPTION = 2;

} // namespace

using index_type = osmium::index::map::Map<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;


class GenericOGRHandler : public osmium::handler::Handler {

protected:

    osmium::geom::OGRFactory<> ogr_factory;

    std::set<std::string> tags;

    void setTagsOfFeature(gdalcpp::Feature &feature, const osmium::OSMObject &osmobj) {
        for(const std::string& tag : this->tags) {
            feature.set_field(tag.c_str(), osmobj.tags().get_value_by_key(tag.c_str()));
        }
    }

    void addTagFieldsToLayer(gdalcpp::Layer &layer) {
        for(const std::string& tag : this->tags) {
            layer.add_field(tag, OFTString, 100);
        }
    }

    void addDefaultFieldsToLayer(gdalcpp::Layer &layer) {
        layer.add_field("id", OFTReal, 10);
    }

    void setDefaultFieldsOfFeature(gdalcpp::Feature &feature, const osmium::OSMObject &osmobj) {
        feature.set_field("id", static_cast<double>(osmobj.id()));
    }

    void setExportTags(std::vector<std::string> &tags) {
        this->tags.erase(this->tags.begin(), this->tags.end());
        std::copy(tags.begin(), tags.end(), std::inserter(this->tags, this->tags.end()));
    }

public: 
};


class NodeOGRHandler : public GenericOGRHandler {

protected:
    gdalcpp::Layer layer;

public:

    explicit NodeOGRHandler(gdalcpp::Dataset& dataset, std::vector<std::string> &tags) :
        layer(dataset, "nodes", wkbPoint) {

        setExportTags(tags);
        addDefaultFieldsToLayer(layer);
        addTagFieldsToLayer(layer);

    }

    void node(const osmium::Node& node) {
        gdalcpp::Feature feature{layer, ogr_factory.create_point(node)};
        setDefaultFieldsOfFeature(feature, node);
        setTagsOfFeature(feature, node);
        feature.add_to_layer();
    }
};



class WayOGRHandler : public GenericOGRHandler {

protected:
    gdalcpp::Layer layer;
    bool includeLength;

public:

    explicit WayOGRHandler(gdalcpp::Dataset& dataset, std::vector<std::string> &tags, bool includeLength) :
        layer(dataset, "ways", wkbLineString),
        includeLength(includeLength) {

        setExportTags(tags);
        addDefaultFieldsToLayer(layer);
        if (includeLength) {
            layer.add_field(LENGTH_FIELD_NAME, OFTReal, 10);
        }
        addTagFieldsToLayer(layer);

    }

    void way(const osmium::Way& way) {
        gdalcpp::Feature feature{layer, ogr_factory.create_linestring(way)};
        setDefaultFieldsOfFeature(feature, way);
        if (includeLength) {
            feature.set_field(LENGTH_FIELD_NAME, osmium::geom::haversine::distance(way.nodes()));
        }
        setTagsOfFeature(feature, way);
        feature.add_to_layer();
    }
};



int main(int argc, char* argv[]) {

    try {
        std::vector<std::string> tags;
        std::string outputfile_name;
        std::string inputfile_name;
        std::string outputfile_format = "ESRI Shapefile";
        bool convertWays = false;
        bool includeLength = false;

        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("help,h", "Print help messages")
            ("version,v", "Print version and exit")
            ("length", "Add a field containing the length of features. The units are meters. "
                        "The name of the field will be \"" LENGTH_FIELD_NAME "\". "
                        "This option only applies when ways are expored.")
            ("tag,t", po::value<std::vector<std::string> >(&tags), "Tags to create columns for. This option "
                        "may be used multiple times to add more than one tag.")
            ("ways,w", "convert ways instead of nodes. Default is nodes.")
            ("outputfile,o", po::value<std::string>(&outputfile_name)->required(), "Name of the output file")
            ("inputfile,i", po::value<std::string>(&inputfile_name)->required(), "Name of the input file");

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
            gdalcpp::Dataset dataset{outputfile_format, outputfile_name, gdalcpp::SRS{}, {}};

            if (convertWays) {
                auto handler = WayOGRHandler(dataset, tags, includeLength);
                osmium::apply(reader, location_handler, handler);
            }
            else {
                auto handler = NodeOGRHandler(dataset, tags);
                osmium::apply(reader, location_handler, handler);
            }
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
