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
    std::string state_file;        // Новое
    int save_state_period = 0;     // Новое
};

[[nodiscard]] inline std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"};
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions")
        ("state-file", po::value(&args.state_file)->value_name("file"), "set state file path")
        ("save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"), "set auto-save state period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.contains("config-file")) {
        throw std::runtime_error("Config file path is not specified");
    }

    if (!vm.contains("www-root")) {
        throw std::runtime_error("Static files root is not specified");
    }

    return args;
}