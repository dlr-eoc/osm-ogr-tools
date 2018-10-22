#include "boost/program_options.hpp"

#include <iostream>
#include <string>
#include <vector>

#include "version.hpp"

#define PROGRAM_NAME "osm2ogr_tagged"

namespace
{
  const size_t ERROR_WRONG_ARGUMENTS = 1;
  const size_t SUCCESS = 0;
  const size_t ERROR_UNHANDLED_EXCEPTION = 2;

} // namespace


int main(int argc, char* argv[]) {

    try {
        std::vector<std::string> tags;
        std::string outputfile_name;
        std::string inputfile_name;

        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("help,h", "Print help messages")
            ("version,v", "Print version and exit")
            ("length", "Add a field containing the length of features. The units are meters.")
            ("tag", po::value<std::vector<std::string> >(&tags), "Tags to create columns for. This optiom may be used multiple times to add more than one tag.")
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

        // application code here //
        for(const std::string& tag : tags) {
            std::cout << "TAG: " << tag << std::endl;
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
