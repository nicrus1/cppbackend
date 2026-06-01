#pragma once
#include <boost/program_options.hpp>
#include <optional>
#include <string>
#include <iostream>
#include <stdexcept>

struct Args {
    int tick_period = 0;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

[[nodiscard]] inline std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"};
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file")->required(), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir")->required(), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        
        if (vm.contains("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }
        
        po::notify(vm);
    } catch (const po::error& e) {
        throw std::runtime_error(std::string("Command line error: ") + e.what());
    }

    return args;
}