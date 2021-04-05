#pragma once

#include <vector>
#include <optional>

#include "d_model_data.h"


namespace dal {

    bool parse_model_dmd(ModelStatic& output, const uint8_t* const data, const size_t data_size);

    std::optional<ModelStatic> parse_model_dmd(const uint8_t* const data, const size_t data_size);

}
