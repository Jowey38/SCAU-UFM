#pragma once

#include <cstddef>
#include <string>

#include "coupling/drainage/swmm_boundary.hpp"

namespace scau::coupling::drainage {

// Real embedded SWMM 5.2.4 engine (extern/swmm5, statically linked).
//
// Boundary contract (architecture spec + project-layout-design.md firewall):
// - Lifecycle and state read/write only. No Q_limit / V_limit / deficit /
//   rollback / replay / arbitration semantics live here; those are core-owned.
// - swmm5.h is included only in src/swmm_adapter/swmm_engine.cpp; no
//   third-party type leaks through this header or any DTO.
// - The SWMM solver keeps process-wide global state, so at most one
//   SwmmEngine may be open at a time; a second concurrent instance's
//   initialize() fails closed.
//
// Units: all values pass through in the unit system of the loaded .inp
// project. SCAU-UFM requires metric projects (FLOW_UNITS CMS), giving SI
// units at this boundary (m, m3/s).
//
// Compiled only when SCAU_EMBED_SWMM is ON (definition SCAU_HAS_SWMM5).
class SwmmEngine final : public ISwmmEngine {
public:
    SwmmEngine() = default;
    ~SwmmEngine() override;

    SwmmEngine(const SwmmEngine&) = delete;
    SwmmEngine& operator=(const SwmmEngine&) = delete;
    SwmmEngine(SwmmEngine&&) = delete;
    SwmmEngine& operator=(SwmmEngine&&) = delete;

    void initialize(const std::string& inp_path) override;
    void step(core::Real dt_swmm) override;
    void finalize() override;

    [[nodiscard]] core::Real get_node_head(std::size_t node_id) const override;
    [[nodiscard]] core::Real get_node_lateral_inflow(std::size_t node_id) const override;
    void set_node_lateral_inflow(std::size_t node_id, core::Real q) override;
    [[nodiscard]] core::Real get_node_inflow(std::size_t node_id) const;
    [[nodiscard]] core::Real get_node_overflow(std::size_t node_id) const;
    void set_outfall_stage(std::size_t node_id, core::Real stage);

    [[nodiscard]] core::Real get_link_flow(std::size_t link_id) const override;
    [[nodiscard]] bool is_surcharged(std::size_t node_id) const override;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] core::Real elapsed_time() const noexcept;  // seconds since start
    [[nodiscard]] std::size_t node_count() const;
    [[nodiscard]] std::size_t node_index(const std::string& node_name) const;
    [[nodiscard]] std::size_t link_index(const std::string& link_name) const;

private:
    void require_initialized() const;
    void require_valid_node(std::size_t node_id) const;
    void require_valid_link(std::size_t link_id) const;

    bool initialized_{false};
    double elapsed_days_{0.0};
    bool simulation_ended_{false};
};

}  // namespace scau::coupling::drainage
