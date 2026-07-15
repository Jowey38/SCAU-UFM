#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "coupling/drainage/swmm_engine.hpp"

namespace scau::coupling::drainage {

class MockSwmmEngine final : public ISwmmEngine {
public:
    explicit MockSwmmEngine(std::size_t node_count, std::size_t link_count = 0U);

    void initialize(const std::string& inp_path) override;
    void step(core::Real dt_swmm) override;
    void finalize() override;

    [[nodiscard]] core::Real get_node_head(std::size_t node_id) const override;
    [[nodiscard]] core::Real get_node_lateral_inflow(std::size_t node_id) const override;
    void set_node_lateral_inflow(std::size_t node_id, core::Real q) override;

    [[nodiscard]] core::Real get_link_flow(std::size_t link_id) const override;
    [[nodiscard]] bool is_surcharged(std::size_t node_id) const override;

    void set_node_head(std::size_t node_id, core::Real head);
    void set_surcharged(std::size_t node_id, bool surcharged);
    void set_link_flow(std::size_t link_id, core::Real flow);

    [[nodiscard]] core::Real elapsed_time() const;
    [[nodiscard]] bool initialized() const;

private:
    void validate_node(std::size_t node_id) const;
    void validate_link(std::size_t link_id) const;

    std::vector<core::Real> node_heads_;
    std::vector<core::Real> node_lateral_inflows_;
    std::vector<bool> node_surcharged_;
    std::vector<core::Real> link_flows_;
    core::Real elapsed_time_{0.0};
    bool initialized_{false};
};

}  // namespace scau::coupling::drainage
