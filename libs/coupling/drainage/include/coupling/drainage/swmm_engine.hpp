#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "core/types.hpp"

namespace scau::coupling::drainage {

class SwmmEngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ISwmmEngine {
public:
    virtual ~ISwmmEngine() = default;

    virtual void initialize(const std::string& inp_path) = 0;
    virtual void step(core::Real dt_swmm) = 0;
    virtual void finalize() = 0;

    [[nodiscard]] virtual core::Real get_node_head(std::size_t node_id) const = 0;
    [[nodiscard]] virtual core::Real get_node_lateral_inflow(std::size_t node_id) const = 0;
    virtual void set_node_lateral_inflow(std::size_t node_id, core::Real q) = 0;

    [[nodiscard]] virtual core::Real get_link_flow(std::size_t link_id) const = 0;
    [[nodiscard]] virtual bool is_surcharged(std::size_t node_id) const = 0;
};

}  // namespace scau::coupling::drainage
