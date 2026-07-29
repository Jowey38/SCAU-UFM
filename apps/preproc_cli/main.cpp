#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "stcf/case_profiles.hpp"
#include "stcf/io_netcdf.hpp"
#include "stcf/topology.hpp"

namespace {

struct Options {
    std::string profile;
    std::filesystem::path output;
    bool force{false};
};

[[noreturn]] void usage_error(const std::string& message) {
    throw std::invalid_argument(
        message
        + "\nusage: scau_preproc generate --profile <name> --output <case.stcf.nc> [--force]");
}

Options parse_options(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "generate") {
        usage_error("expected 'generate' command");
    }
    Options options;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--profile") {
            if (!options.profile.empty() || index + 1 >= argc) {
                usage_error("--profile must appear once with a value");
            }
            options.profile = argv[++index];
        } else if (argument == "--output") {
            if (!options.output.empty() || index + 1 >= argc) {
                usage_error("--output must appear once with a value");
            }
            options.output = argv[++index];
        } else if (argument == "--force") {
            if (options.force) {
                usage_error("--force must not be repeated");
            }
            options.force = true;
        } else {
            usage_error("unknown argument: " + argument);
        }
    }
    if (options.profile.empty()) {
        usage_error("missing --profile");
    }
    if (options.output.empty()) {
        usage_error("missing --output");
    }
    return options;
}

void generate(const Options& options) {
    if (std::filesystem::exists(options.output) && !options.force) {
        throw std::runtime_error(
            "output already exists (use --force to replace): " + options.output.string());
    }
    const auto parent = options.output.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const auto stcf_case = scau::stcf::make_case_profile(options.profile);
    scau::stcf::validate_stcf_case(stcf_case);

    auto temporary = options.output;
    temporary += ".tmp";
    std::filesystem::remove(temporary);
    try {
        scau::stcf::write_stcf_case(temporary, stcf_case);
        if (options.force) {
            std::filesystem::remove(options.output);
        }
        std::filesystem::rename(temporary, options.output);
    } catch (...) {
        std::filesystem::remove(temporary);
        throw;
    }

    std::cout << "wrote STCF v5 UGRID case: " << options.output.string()
              << " (profile=" << options.profile
              << ", nodes=" << stcf_case.topology.node_x.size()
              << ", faces=" << stcf_case.topology.face_nodes.size()
              << ", edges=" << stcf_case.topology.edge_nodes.size() << ")\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        generate(parse_options(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
