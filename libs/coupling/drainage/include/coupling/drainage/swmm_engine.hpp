#pragma once

#include <string>

#include "coupling/drainage/swmm_boundary.hpp"

namespace scau::coupling::drainage {

// Real embedded SWMM 5.2.4 engine (extern/swmm5, statically linked).
//
// Boundary contract (architecture spec + project-layout-design.md firewall):
// - Lifecycle and state read/write only. No Q_limit / V_limit / deficit /
//   rollback / replay / arbitration semantics live here; those are core-owned.
// - swmm5.h is included only in the .cpp; no third-party type leaks through
//   this header or any DTO.
// - The SWMM solver keeps process-wide global state, so at most one
//   SwmmEngine may be open at a time; the constructor of a second concurrent
//   instance's initialize() fails closed.
//
// Units: all values pass through in the unit system of the loaded .inp
// project. SCAU-UFM requires metric projects (FLOW_UNITS CMS), giving SI
// units at this boundary (m, m3/s).
class SwmmEngine final : public ISwmmEngine {
public:
    SwmmEngine() = default;
    ~SwmmEngine() override;

    SwmmEngine(const SwmmEngine&) = delete;
    SwmmEngine& operator=(const SwmmEngine&) = delete;
    SwmmEngine(SwmmEngine&&) = delete;
    SwmmEngine& operator=(SwmmEngine&&) = delete;

    void initialize(const std::string& inp_path) override;
    void step(double dt_swmm) override;
    void finalize() override;

    [[nodiscard]] double get_node_head(int node_id) const override;
    [[nodiscard]] double get_node_lateral_inflow(int node_id) const override;
    void set_node_lateral_inflow(int node_id, double q) override;
    [[nodiscard]] double get_node_inflow(int node_id) const;
    [[nodiscard]] double get_node_overflow(int node_id) const;
    void set_outfall_stage(int node_id, double stage);

    [[nodiscard]] double get_link_flow(int link_id) const override;
    [[nodiscard]] bool is_surcharged(int node_id) const override;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] double elapsed_time() const noexcept;  // seconds since start
    [[nodiscard]] int node_count() const;
    [[nodiscard]] int node_index(const std::string& node_name) const;
    [[nodiscard]] int link_index(const std::string& link_name) const;

private:
    void require_initialized() const;
    void require_valid_node(int node_id) const;
    void require_valid_link(int link_id) const;

    bool initialized_{false};
    double elapsed_days_{0.0};
    bool simulation_ended_{false};
};

[[nodiscard]] core::EngineReport make_swmm_engine_report(const SwmmEngine& engine);

}  // namespace scau::coupling::drainage
