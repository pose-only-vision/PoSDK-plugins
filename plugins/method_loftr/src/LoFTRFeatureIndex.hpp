#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PluginMethods {

// Fixed representatives avoid transitive merging through a chain of nearby
// observations. Every image owns its index, independently of image pairs.
class LoFTRFeatureIndex {
public:
    explicit LoFTRFeatureIndex(double radius) : radius_(radius) {
        if (!std::isfinite(radius) || radius <= 0)
            throw std::invalid_argument("Invalid LoFTR feature merge radius");
    }

    std::size_t FindOrAdd(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y) ||
            std::abs(double(x) / radius_) > 1e12 ||
            std::abs(double(y) / radius_) > 1e12)
            throw std::invalid_argument("Invalid LoFTR keypoint coordinates");
        const Cell cell{static_cast<std::int64_t>(std::floor(x / radius_)),
                        static_cast<std::int64_t>(std::floor(y / radius_))};
        auto best = std::numeric_limits<std::size_t>::max();
        double best_distance = radius_ * radius_;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                const auto found = cells_.find({cell.first + dx, cell.second + dy});
                if (found == cells_.end()) continue;
                for (auto id : found->second) {
                    const double ex = double(x) - points_[id].first;
                    const double ey = double(y) - points_[id].second;
                    const double distance = ex * ex + ey * ey;
                    if (distance < best_distance ||
                        (distance == best_distance && id < best)) {
                        best_distance = distance;
                        best = id;
                    }
                }
            }
        }
        if (best != std::numeric_limits<std::size_t>::max()) return best;
        const auto id = points_.size();
        points_.emplace_back(x, y);
        cells_[cell].push_back(id);
        return id;
    }

    const std::vector<std::pair<float, float>> &Points() const { return points_; }

private:
    using Cell = std::pair<std::int64_t, std::int64_t>;
    struct Hash {
        std::size_t operator()(const Cell &cell) const {
            const auto a = std::hash<std::int64_t>{}(cell.first);
            const auto b = std::hash<std::int64_t>{}(cell.second);
            return a ^ (b + 0x9e3779b9U + (a << 6U) + (a >> 2U));
        }
    };
    double radius_;
    std::vector<std::pair<float, float>> points_;
    std::unordered_map<Cell, std::vector<std::size_t>, Hash> cells_;
};

} // namespace PluginMethods
