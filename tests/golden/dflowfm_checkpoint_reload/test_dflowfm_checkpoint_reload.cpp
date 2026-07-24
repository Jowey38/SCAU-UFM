#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "coupling/driver/dflowfm_checkpoint.hpp"

namespace {

std::string environment_value(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, name) != 0 || value == nullptr) return {};
    std::string result(value); std::free(value); return result;
#else
    const char* value = std::getenv(name); return value == nullptr ? std::string{} : std::string(value);
#endif
}

}  // namespace

TEST(GoldenDFlowFMCheckpointReload, RealEngineReloadsNativeCheckpointAtLogicalTime) {
    const std::string library = environment_value("SCAU_DFLOWFM_LIBRARY");
    const std::string base_mdu = environment_value("SCAU_DFLOWFM_G11_MDU");
    const std::string checkpoint_file = environment_value("SCAU_DFLOWFM_G11_CHECKPOINT_600");
    if (library.empty() || base_mdu.empty() || checkpoint_file.empty()) {
        GTEST_SKIP() << "real checkpoint reload requires D-Flow FM runtime/checkpoint env";
    }

    const std::filesystem::path generated =
        std::filesystem::temp_directory_path() / "scau_dflowfm_restart_600.mdu";
    scau::coupling::river::DFlowFMEngine engine(library);
    engine.initialize(base_mdu);
    engine.update(60.0);
    EXPECT_DOUBLE_EQ(engine.current_time(), 60.0);

    const scau::coupling::driver::DFlowFMCheckpoint checkpoint{
        .logical_time = 600.0,
        .source = scau::coupling::driver::DFlowFMCheckpointSource::restart_nc,
        .state_file = checkpoint_file,
        .base_mdu = base_mdu,
        .model_fingerprint = "scau-authored-single-reach-v1",
    };
    scau::coupling::driver::reload_dflowfm_from_checkpoint(engine, checkpoint, generated);

    ASSERT_TRUE(engine.initialized());
    EXPECT_DOUBLE_EQ(engine.current_time(), 600.0);
    engine.update(60.0);
    EXPECT_DOUBLE_EQ(engine.current_time(), 660.0);
    EXPECT_TRUE(std::isfinite(engine.get_value("s1", 0)));
    engine.finalize();
    std::filesystem::remove(generated);
}
