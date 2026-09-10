#include <filesystem>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "coupling/drainage/swmm_engine.hpp"

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
        + "\nusage: scau_preproc generate --profile <name> --output <case.stcf.nc> [--force]"
        + "\n       scau_preproc validate --input <case.stcf.nc>"
        + "\n       scau_preproc swmm-parse --inp <file.inp>");
}

// Authoritative external check: read_stcf_case re-runs the full STCF v5
// dataset + UGRID topology validation on load, so a zero exit here certifies
// the file against the same rules the solver-loading path enforces (M287-A:
// externally generated meshes are certified through this single validator,
// never through a re-implemented copy).
int validate_command(int argc, char** argv) {
    std::filesystem::path input;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--input") {
            if (!input.empty() || index + 1 >= argc) {
                usage_error("--input must appear once with a value");
            }
            input = argv[++index];
        } else {
            usage_error("unknown argument: " + argument);
        }
    }
    if (input.empty()) {
        usage_error("missing --input");
    }
    const auto stcf_case = scau::stcf::read_stcf_case(input);
    std::cout << "valid STCF v5 UGRID case: " << input.string()
              << " (nodes=" << stcf_case.topology.node_x.size()
              << ", faces=" << stcf_case.topology.face_nodes.size()
              << ", edges=" << stcf_case.topology.edge_nodes.size()
              << ", soil_entries=" << stcf_case.fields.soil_params.size() << ")\n";
    return 0;
}

Options parse_options(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "generate") {
        usage_error("expected 'generate' or 'validate' command");
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

int swmm_parse_command(int argc, char** argv) {
    std::filesystem::path input;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--inp" && input.empty() && index + 1 < argc) {
            input = argv[++index];
        } else {
            usage_error("usage: scau_preproc swmm-parse --inp <file.inp>");
        }
    }
    if (input.empty()) {
        usage_error("missing --inp");
    }
#ifdef SCAU_HAS_SWMM5
    const auto scratch = std::filesystem::temp_directory_path() / "scau_preproc_swmm_parse";
    std::filesystem::create_directories(scratch);
    const auto stem = std::to_string(reinterpret_cast<std::uintptr_t>(&input));
    const auto report = scratch / (stem + ".rpt");
    const auto output = scratch / (stem + ".out");
    scau::coupling::drainage::SwmmEngine engine;
    try {
        engine.initialize(input.string(), report.string(), output.string());
        std::cout << "{\"engine\":\"swmm 5.2.4\",\"ok\":true,\"node_count\":"
                  << engine.node_count() << ",\"link_count\":" << engine.link_count() << ",\"nodes\":[";
        for (int node = 0; node < engine.node_count(); ++node) {
            if (node != 0) std::cout << ',';
            std::cout << "{\"id\":\"" << engine.node_name(node) << "\",\"invert_elevation_m\":"
                      << engine.node_invert_elevation(node) << "}";
        }
        std::cout << "]}\n";
        engine.finalize();
    } catch (...) {
        engine.finalize();
        std::filesystem::remove(report);
        std::filesystem::remove(output);
        throw;
    }
    std::filesystem::remove(report);
    std::filesystem::remove(output);
    return 0;
#else
    throw std::runtime_error("SWMM support is not embedded in this build");
#endif
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
        if (argc >= 2 && std::string(argv[1]) == "validate") {
            return validate_command(argc, argv);
        }
        if (argc >= 2 && std::string(argv[1]) == "swmm-parse") {
            return swmm_parse_command(argc, argv);
        }
        generate(parse_options(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
