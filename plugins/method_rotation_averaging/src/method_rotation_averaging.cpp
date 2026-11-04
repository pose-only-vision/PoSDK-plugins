/**
 * @file method_rotation_averaging.cpp
 * @brief Rotation averaging method implementation | 旋转平均方法实现
  * @copyright Copyright (c) 2024 Qi Cai
 * @author qi Cai
 * SPDX-License-Identifier: LicenseRef-Custom
*/

#include "method_rotation_averaging.hpp"
#include "../plugin_runtime_path.hpp"
#include <po_core/po_logger.hpp>
#include <po_core/ProfilerManager.hpp> // Profiler system | 性能分析系统
#include <po_core/methods/RobustLiGTStableR2Contract.hpp>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <set>
#include <queue>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <Eigen/Geometry> // For Eigen::Quaterniond
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

namespace PoSDK
{

    namespace
    {
        std::set<ViewId> RelativePoseViews(const RelativePoses &poses)
        {
            std::set<ViewId> views;
            for (const auto &pose : poses)
            {
                views.insert(pose.GetViewIdI());
                views.insert(pose.GetViewIdJ());
            }
            return views;
        }

        RelativeRotations RotationOnlyCopy(const RelativePoses &poses)
        {
            RelativeRotations rotations;
            rotations.reserve(poses.size());
            for (const auto &pose : poses)
            {
                if (pose.GetViewIdI() == pose.GetViewIdJ())
                    continue;
                rotations.emplace_back(
                    pose.GetViewIdI(), pose.GetViewIdJ(), pose.GetRotation(),
                    pose.GetWeight());
            }
            return rotations;
        }
    } // namespace

    bool MethodRotationAveraging::ApplyRotationGraphFilter(
        RelativePoses &relative_poses,
        RelativePoses::RotationGraphFilterResult &result,
        std::string &mode,
        const GlobalPoses *scoring_prior) const
    {
        mode = GetOptionAsString("rotation_graph_filter_mode", "none");
        if (!GetOptionAsBool("enable_rotation_graph_filter", false))
            mode = "none";
        if (mode == "scored")
            mode = "scored_backbone";
        if (scoring_prior != nullptr && mode != "scored_backbone")
        {
            LOG_ERROR_EN << "[MethodRotationAveraging] prior rotation scoring requires scored_backbone graph filter";
            return false;
        }

        const auto views_before = RelativePoseViews(relative_poses);
        result.total_edges_before = relative_poses.size();
        result.total_edges_after = relative_poses.size();
        result.edges_removed = 0U;
        result.covered_views_before = views_before.size();
        result.covered_views_after = views_before.size();

        if (mode == "none")
        {
            LOG_INFO_ALL << "RA_GRAPH_FILTER mode=none before="
                         << result.total_edges_before << " after="
                         << result.total_edges_after << " removed=0 views="
                         << result.covered_views_before << "->"
                         << result.covered_views_after;
            return !relative_poses.empty();
        }

        const double residual_threshold = GetOptionAsDouble(
            "rotation_graph_residual_threshold_deg", 0.5);
        const double cauchy_scale = GetOptionAsDouble(
            "rotation_graph_cauchy_scale_deg", 2.0);
        const double initial_cauchy_scale = GetOptionAsDouble(
            "rotation_graph_initial_cauchy_scale_deg", -1.0);
        const size_t max_iterations = static_cast<size_t>(std::max<IndexT>(
            1, GetOptionAsIndexT("rotation_graph_max_iterations", 8)));
        const bool reject_bridges = GetOptionAsBool(
            "rotation_graph_reject_non_biconnected", false);
        const bool preserve_connectivity = GetOptionAsBool(
            "rotation_graph_preserve_connectivity", true);
        const bool keep_largest = GetOptionAsBool(
            "rotation_graph_keep_largest_component", false);

        if (mode == "global_sync" || mode == "global_sync_mad")
        {
            if (mode == "global_sync")
            {
                result = relative_poses.FilterByRotationGraphConsistency(
                    residual_threshold, cauchy_scale, max_iterations,
                    reject_bridges, preserve_connectivity, keep_largest,
                    initial_cauchy_scale);
            }
            else
            {
                RelativePoses::RotationGraphMADOptions mad;
                mad.enabled = true;
                mad.sigma_multiplier = GetOptionAsDouble(
                    "rotation_graph_mad_sigma_multiplier", 3.0);
                mad.minimum_threshold_deg = GetOptionAsDouble(
                    "rotation_graph_mad_minimum_threshold_deg", 0.5);
                mad.maximum_threshold_deg = GetOptionAsDouble(
                    "rotation_graph_mad_maximum_threshold_deg", 5.0);
                mad.minimum_component_edges = static_cast<size_t>(
                    std::max<IndexT>(3, GetOptionAsIndexT(
                        "rotation_graph_mad_minimum_component_edges", 8)));
                result = relative_poses.FilterByRotationGraphConsistency(
                    residual_threshold, cauchy_scale, max_iterations,
                    reject_bridges, preserve_connectivity, keep_largest,
                    initial_cauchy_scale, mad);
            }
        }
        else if (mode == "local_cycle" || mode == "coverage_cycle")
        {
            const double cycle_threshold = GetOptionAsDouble(
                "rotation_graph_cycle_threshold_deg",
                mode == "coverage_cycle" ? 2.0 : 3.0);
            const size_t neighbor_checks = static_cast<size_t>(std::max<IndexT>(
                1, GetOptionAsIndexT(
                    "rotation_graph_cycle_max_neighbor_checks", 128)));
            const size_t max_cycles = static_cast<size_t>(std::max<IndexT>(
                1, GetOptionAsIndexT(
                    "rotation_graph_cycle_max_cycles_per_edge", 32)));
            if (mode == "coverage_cycle")
            {
                const double bridge_scale = std::clamp(
                    GetOptionAsDouble(
                        "rotation_graph_cycle_unsupported_weight_scale", 0.25),
                    1.0e-3, 1.0);
                result = relative_poses.FilterByCoverageCycle(
                    cycle_threshold, neighbor_checks, max_cycles,
                    bridge_scale, keep_largest);
            }
            else
            {
                const size_t min_support = static_cast<size_t>(std::max<IndexT>(
                    1, GetOptionAsIndexT("rotation_graph_cycle_min_support", 3)));
                const double min_ratio = std::clamp(
                    GetOptionAsDouble(
                        "rotation_graph_cycle_min_consistent_ratio", 0.5),
                    0.0, 1.0);
                const double unsupported_scale = std::clamp(
                    GetOptionAsDouble(
                        "rotation_graph_cycle_unsupported_weight_scale", 0.25),
                    1.0e-3, 1.0);
                result = relative_poses.FilterByLocalCycleConsistency(
                    cycle_threshold, neighbor_checks, max_cycles, min_support,
                    min_ratio, reject_bridges, preserve_connectivity,
                    keep_largest, unsupported_scale);
            }
        }
        else if (mode == "scored_backbone")
        {
            RelativePoses::ScoredBackboneOptions options;
            options.chord_residual_threshold_deg = GetOptionAsDouble(
                "rotation_graph_backbone_chord_threshold_deg", 2.0);
            options.cauchy_scale_deg = cauchy_scale;
            options.max_iterations = max_iterations;
            options.keep_largest_component = keep_largest;
            options.initial_cauchy_scale_deg = initial_cauchy_scale;
            options.cover_bridges = GetOptionAsBool(
                "rotation_graph_cover_bridges", false);
            options.strip_uncovered_bridges = GetOptionAsBool(
                "rotation_graph_strip_uncovered_bridges", false);
            options.covering_chord_residual_threshold_deg = GetOptionAsDouble(
                "rotation_graph_covering_chord_threshold_deg", -1.0);
            options.use_sparse_partial_eigensolver = GetOptionAsBool(
                "rotation_graph_sparse_partial_eigensolver", false);
            if (scoring_prior != nullptr)
            {
                options.use_prior_rotations_for_scoring = true;
                for (const ViewId view : views_before)
                {
                    if (view < scoring_prior->Size())
                        options.prior_rotations.emplace(
                            view, scoring_prior->GetRotation(view));
                }
            }
            result = relative_poses.FilterByScoredBackbone(options);
        }
        else if (mode == "local_cycle_then_scored")
        {
            const double cycle_threshold = GetOptionAsDouble(
                "rotation_graph_cycle_threshold_deg", 10.0);
            const size_t neighbor_checks = static_cast<size_t>(std::max<IndexT>(
                1, GetOptionAsIndexT(
                    "rotation_graph_cycle_max_neighbor_checks", 128)));
            const size_t max_cycles = static_cast<size_t>(std::max<IndexT>(
                1, GetOptionAsIndexT(
                    "rotation_graph_cycle_max_cycles_per_edge", 32)));
            const size_t min_support = static_cast<size_t>(std::max<IndexT>(
                1, GetOptionAsIndexT("rotation_graph_cycle_min_support", 3)));
            const double min_ratio = std::clamp(
                GetOptionAsDouble(
                    "rotation_graph_cycle_min_consistent_ratio", 0.5),
                0.0, 1.0);
            const double unsupported_scale = std::clamp(
                GetOptionAsDouble(
                    "rotation_graph_cycle_unsupported_weight_scale", 0.25),
                1.0e-3, 1.0);
            const auto cycle_result = relative_poses.FilterByLocalCycleConsistency(
                cycle_threshold, neighbor_checks, max_cycles, min_support,
                min_ratio, reject_bridges, preserve_connectivity,
                keep_largest, unsupported_scale);
            RelativePoses::ScoredBackboneOptions options;
            options.chord_residual_threshold_deg = GetOptionAsDouble(
                "rotation_graph_backbone_chord_threshold_deg", 1.0);
            options.cauchy_scale_deg = cauchy_scale;
            options.max_iterations = max_iterations;
            options.keep_largest_component = keep_largest;
            options.initial_cauchy_scale_deg = initial_cauchy_scale;
            options.cover_bridges = GetOptionAsBool(
                "rotation_graph_cover_bridges", false);
            options.strip_uncovered_bridges = GetOptionAsBool(
                "rotation_graph_strip_uncovered_bridges", false);
            options.covering_chord_residual_threshold_deg = GetOptionAsDouble(
                "rotation_graph_covering_chord_threshold_deg", -1.0);
            result = relative_poses.FilterByScoredBackbone(options);
            result.total_edges_before = cycle_result.total_edges_before;
            result.edges_removed = result.total_edges_before > result.total_edges_after
                ? result.total_edges_before - result.total_edges_after : 0U;
        }
        else
        {
            LOG_ERROR_EN << "[MethodRotationAveraging] Unknown rotation graph filter mode: "
                         << mode;
            LOG_ERROR_ZH << "[MethodRotationAveraging] 未知旋转图过滤模式: "
                         << mode;
            return false;
        }

        result.covered_views_after = RelativePoseViews(relative_poses).size();
        LOG_INFO_ALL << "RA_GRAPH_FILTER mode=" << mode
                     << " before=" << result.total_edges_before
                     << " after=" << result.total_edges_after
                     << " removed=" << result.edges_removed
                     << " views=" << result.covered_views_before << "->"
                     << result.covered_views_after
                     << " components=" << result.connected_components_before
                     << "->" << result.connected_components_after;
        return !relative_poses.empty();
    }

    bool MethodRotationAveraging::ApplyFixedCoreCommit(
        const GlobalPoses &candidate,
        const GlobalPoses &prior,
        const types::PoseGraphQuality &prior_quality,
        const Tracks &tracks,
        const RelativePoses &filtered_relative_poses,
        GlobalPoses &output,
        types::PoseGraphQuality &output_quality,
        bool update_certificate) const
    {
        const std::vector<ViewId> core_views =
            stable_r2::contract::CertifiedCoreViews(prior_quality);
        if (core_views.size() < 3U || prior_quality.views.size() != prior.Size())
        {
            LOG_ERROR_EN << "[MethodRotationAveraging] core_view_fixed_mode requires a certified Core role table";
            LOG_ERROR_ZH << "[MethodRotationAveraging] core_view_fixed_mode要求有效的Core角色证书";
            return false;
        }
        for (const ViewId view : core_views)
        {
            if (view >= prior.Size() || !prior.GetRotation(view).allFinite())
                return false;
        }

        output = prior;
        output.EnsureViewCapacity(candidate.Size());
        std::set<ViewId> core_set(core_views.begin(), core_views.end());
        ViewId anchor = EstInfo::kInvalidViewId;
        for (const ViewId view : core_views)
        {
            if (view < candidate.Size() && candidate.GetRotation(view).allFinite() &&
                candidate.GetViewState(view) != EstInfo::ViewState::FILTERED)
            {
                anchor = view;
                break;
            }
        }
        if (anchor == EstInfo::kInvalidViewId)
        {
            LOG_ERROR_EN << "[MethodRotationAveraging] fixed Core has no common RA anchor";
            LOG_ERROR_ZH << "[MethodRotationAveraging] 固定Core与RA图没有共同锚点";
            return false;
        }

        // R_j = Rij R_i, therefore the gauge acts on the right: R'_i=R_i Q.
        const Matrix3d gauge = candidate.GetRotation(anchor).transpose() *
                               prior.GetRotation(anchor);
        if (!gauge.allFinite())
            return false;

        // Solve only the non-Core rotations while treating every certified
        // Core rotation as an immutable boundary condition.  A full RA solve
        // followed by copying Frontier rotations would allow a non-rigid Core
        // drift to leak into the expansion equations.
        struct NeighborConstraint
        {
            ViewId neighbor = 0;
            Matrix3d transform = Matrix3d::Identity();
            double weight = 0.0;
        };
        std::set<ViewId> graph_views;
        for (const auto &pose : filtered_relative_poses)
        {
            graph_views.insert(pose.GetViewIdI());
            graph_views.insert(pose.GetViewIdJ());
        }
        std::map<ViewId, Matrix3d> rotations;
        std::map<ViewId, std::vector<NeighborConstraint>> adjacency;
        const bool protect_filtered_prior =
            GetOptionAsBool("rotation_graph_score_from_prior_rotations", false) &&
            GetOptionAsBool(
                "rotation_graph_fixed_core_protect_filtered_prior", false);
        const double filtered_prior_conflict_deg = std::clamp(
            GetOptionAsDouble(
                "rotation_graph_fixed_core_filtered_prior_conflict_deg",
                15.0), 1.0, 90.0);
        std::set<ViewId> protected_filtered_views;
        std::set<ViewId> indirect_filtered_prior_views;
        std::size_t reused_filtered_prior = 0U;
        std::size_t conflicting_core_mode_frozen = 0U;
        double maximum_filtered_conflict_deg = 0.0;
        for (const ViewId view : core_views)
            rotations[view] = prior.GetRotation(view);
        for (const ViewId view : candidate.GetEstimatedOrOptimizedViewIds())
        {
            if (core_set.count(view) != 0U || view >= output.Size())
                continue;
            Matrix3d initial = candidate.GetRotation(view) * gauge;
            if (!initial.allFinite())
                initial = Matrix3d::Identity();
            rotations[view] = initial;
        }
        if (protect_filtered_prior)
        {
            // The certified Core and prior rotations already share the same
            // gauge.  A FILTERED view can still carry a proper rotation even
            // though it is excluded from GetEstimatedOrOptimizedViewIds().
            for (const ViewId view : graph_views)
            {
                if (core_set.count(view) != 0U || view >= prior.Size() ||
                    view >= output.Size() ||
                    prior.GetViewState(view) != EstInfo::ViewState::FILTERED)
                    continue;
                const Matrix3d prior_rotation = prior.GetRotation(view);
                if (!stable_r2::contract::IsProperRotation(prior_rotation))
                    continue;
                rotations[view] = prior_rotation;
                ++reused_filtered_prior;
            }
        }
        // A fixed-Core candidate may intentionally omit a frontier view from
        // EstInfo (e.g. the prior snapshot predates this graph).  Keep such a
        // view in the local solve with a neutral initial rotation; Core-edge
        // hypotheses below replace it before IRLS.  This is also what makes
        // the opt-in prior-scoring fast path capable of recovering new views.
        for (const ViewId view : graph_views)
        {
            if (core_set.count(view) != 0U ||
                rotations.find(view) != rotations.end() ||
                view >= output.Size())
                continue;
            rotations.emplace(view, Matrix3d::Identity());
        }
        for (const auto &pose : filtered_relative_poses)
        {
            if (pose.GetViewIdI() == pose.GetViewIdJ())
                continue;
            const ViewId i = pose.GetViewIdI();
            const ViewId j = pose.GetViewIdJ();
            if (rotations.find(i) == rotations.end() ||
                rotations.find(j) == rotations.end())
                continue;
            const Matrix3d rij = pose.GetRotation();
            const double edge_weight = std::max(1.0e-6,
                static_cast<double>(pose.GetWeight()));
            adjacency[j].push_back({i, rij, edge_weight});
            adjacency[i].push_back({j, rij.transpose(), edge_weight});
        }
        const auto angle_deg = [](const Matrix3d &rotation) {
            const double cosine = std::clamp(
                (rotation.trace() - 1.0) * 0.5, -1.0, 1.0);
            return std::acos(cosine) * 180.0 / M_PI;
        };

        // Core-neighbor hypotheses can form separate rotation modes.  Select
        // the strongest internally consistent mode before IRLS so a mirrored
        // free-RA initialization cannot suppress the correct fixed-Core mode.
        const double mode_threshold = std::max(1.0,
            2.0 * GetOptionAsDouble(
                "rotation_graph_backbone_chord_threshold_deg", 1.0));
        for (const auto &[view, neighbors] : adjacency)
        {
            if (core_set.count(view) != 0U)
                continue;
            double best_support = -1.0;
            Matrix3d best_rotation = rotations.at(view);
            for (const auto &seed : neighbors)
            {
                if (core_set.count(seed.neighbor) == 0U)
                    continue;
                const Matrix3d hypothesis =
                    seed.transform * prior.GetRotation(seed.neighbor);
                double support = 0.0;
                for (const auto &other : neighbors)
                {
                    if (core_set.count(other.neighbor) == 0U)
                        continue;
                    const Matrix3d second =
                        other.transform * prior.GetRotation(other.neighbor);
                    if (angle_deg(hypothesis.transpose() * second) <=
                        mode_threshold)
                        support += other.weight;
                }
                if (support > best_support)
                {
                    best_support = support;
                    best_rotation = hypothesis;
                }
            }
            if (best_support > 0.0)
            {
                if (protect_filtered_prior && view < prior.Size() &&
                    prior.GetViewState(view) == EstInfo::ViewState::FILTERED &&
                    stable_r2::contract::IsProperRotation(
                        prior.GetRotation(view)))
                {
                    const double conflict_deg = angle_deg(
                        prior.GetRotation(view).transpose() * best_rotation);
                    maximum_filtered_conflict_deg = std::max(
                        maximum_filtered_conflict_deg, conflict_deg);
                    if (conflict_deg > filtered_prior_conflict_deg)
                    {
                        // A conflicting Core-neighbor mode is insufficient
                        // evidence to destroy a proper prior Frontier
                        // rotation. Keep it as a fixed boundary in this
                        // commit; its FILTERED lifecycle remains unchanged.
                        rotations[view] = prior.GetRotation(view);
                        protected_filtered_views.insert(view);
                        ++conflicting_core_mode_frozen;
                        continue;
                    }
                }
                rotations[view] = best_rotation;
            }
            else if (protect_filtered_prior && view < prior.Size() &&
                     prior.GetViewState(view) == EstInfo::ViewState::FILTERED &&
                     stable_r2::contract::IsProperRotation(
                         prior.GetRotation(view)))
            {
                // This view can refine through its indirect graph path, but
                // there is no direct Core residual that could justify a
                // large departure from its proper prior orientation.
                indirect_filtered_prior_views.insert(view);
            }
        }
        if (protect_filtered_prior)
            LOG_INFO_ALL << "RA_CORE_FIXED_FILTERED_PRIOR_GUARD enabled=1"
                         << " reused=" << reused_filtered_prior
                         << " conflict_frozen=" << conflicting_core_mode_frozen
                         << " indirect_prior=" << indirect_filtered_prior_views.size()
                         << " max_conflict_deg=" << maximum_filtered_conflict_deg
                         << " threshold_deg=" << filtered_prior_conflict_deg;

        if (GetOptionAsBool("rotation_graph_fixed_core_irls", false))
        {
        // Chatterjee/Govindu's refinement is a sparse tangent-space IRLS
        // solve.  Core rotations are boundary conditions, not variables:
        // every edge contributes three rows with the usual (-I,+I) blocks,
        // while columns are allocated only for anchored Frontier views.  The
        // previous implementation used twenty rounds of quaternion neighbor
        // averaging here.  Apart from being slower on dense scored graphs,
        // that update is not the Chatterjee objective and can let a bad
        // Frontier mode leak through a weak Core edge.
        std::set<ViewId> anchored_views = core_set;
        std::queue<ViewId> pending_anchor;
        for (const ViewId core_view : core_views)
            pending_anchor.push(core_view);
        while (!pending_anchor.empty())
        {
            const ViewId current = pending_anchor.front();
            pending_anchor.pop();
            const auto adjacency_it = adjacency.find(current);
            if (adjacency_it == adjacency.end())
                continue;
            for (const NeighborConstraint &edge : adjacency_it->second)
            {
                if (anchored_views.insert(edge.neighbor).second)
                    pending_anchor.push(edge.neighbor);
            }
        }

        std::vector<ViewId> frontier_views;
        frontier_views.reserve(rotations.size());
        for (const auto &[view, unused_rotation] : rotations)
        {
            (void)unused_rotation;
            if (core_set.count(view) == 0U &&
                anchored_views.count(view) != 0U &&
                protected_filtered_views.count(view) == 0U)
                frontier_views.push_back(view);
        }
        std::sort(frontier_views.begin(), frontier_views.end());
        std::map<ViewId, Eigen::Index> frontier_index;
        for (Eigen::Index index = 0;
             index < static_cast<Eigen::Index>(frontier_views.size());
             ++index)
            frontier_index.emplace(frontier_views[static_cast<size_t>(index)],
                                   index);

        struct IrlsEdge
        {
            ViewId i = 0;
            ViewId j = 0;
            Matrix3d rotation = Matrix3d::Identity();
            double base_weight = 1.0;
        };
        std::vector<IrlsEdge> irls_edges;
        irls_edges.reserve(filtered_relative_poses.size());
        for (const auto &pose : filtered_relative_poses)
        {
            const ViewId i = pose.GetViewIdI();
            const ViewId j = pose.GetViewIdJ();
            if (i == j || rotations.find(i) == rotations.end() ||
                rotations.find(j) == rotations.end())
                continue;
            const bool i_frontier = frontier_index.find(i) != frontier_index.end();
            const bool j_frontier = frontier_index.find(j) != frontier_index.end();
            if (!i_frontier && !j_frontier)
                continue; // Core--Core rows are constants and add no variables.
            const Matrix3d rij = pose.GetRotation();
            if (!rij.allFinite())
                continue;
            const double weight = std::max(
                1.0e-9, static_cast<double>(pose.GetWeight()));
            irls_edges.push_back({i, j, rij, weight});
        }

        if (!frontier_views.empty() && !irls_edges.empty())
        {
            using SparseMatrix = Eigen::SparseMatrix<double>;
            const Eigen::Index row_count =
                static_cast<Eigen::Index>(3U * irls_edges.size());
            const Eigen::Index column_count =
                static_cast<Eigen::Index>(3U * frontier_views.size());
            SparseMatrix mapping(row_count, column_count);
            mapping.reserve(Eigen::VectorXi::Constant(
                column_count, static_cast<int>(6U)));
            for (Eigen::Index edge_index = 0;
                 edge_index < static_cast<Eigen::Index>(irls_edges.size());
                 ++edge_index)
            {
                const IrlsEdge &edge = irls_edges[static_cast<size_t>(edge_index)];
                const auto i_it = frontier_index.find(edge.i);
                const auto j_it = frontier_index.find(edge.j);
                for (Eigen::Index axis = 0; axis < 3; ++axis)
                {
                    const Eigen::Index row = 3 * edge_index + axis;
                    if (i_it != frontier_index.end())
                        mapping.insert(row, 3 * i_it->second + axis) = -1.0;
                    if (j_it != frontier_index.end())
                        mapping.insert(row, 3 * j_it->second + axis) = 1.0;
                }
            }
            mapping.makeCompressed();

            Eigen::SimplicialLDLT<SparseMatrix> linear_solver;
            linear_solver.analyzePattern(mapping.transpose() * mapping);
            const bool analyzed = linear_solver.info() == Eigen::Success;
            const double final_sigma = std::max(
                1.0e-6, GetOptionAsDouble(
                    "rotation_graph_cauchy_scale_deg", 2.0)) * M_PI / 180.0;
            const double configured_initial_sigma = GetOptionAsDouble(
                "rotation_graph_initial_cauchy_scale_deg", -1.0);
            const double initial_sigma =
                std::max(final_sigma,
                         (configured_initial_sigma > 0.0
                              ? configured_initial_sigma * M_PI / 180.0
                              : final_sigma));
            const std::string robust_loss = GetOptionAsString(
                "rotation_graph_fixed_core_irls_loss", "cauchy");
            if (robust_loss != "cauchy" &&
                robust_loss != "geman_mcclure")
            {
                LOG_ERROR_EN << "[MethodRotationAveraging] unknown fixed-Core IRLS loss: "
                             << robust_loss;
                return false;
            }
            const unsigned int max_irls_iterations = static_cast<unsigned int>(
                std::clamp<IndexT>(GetOptionAsIndexT(
                                       "rotation_graph_max_iterations", 8),
                                   1, 32));
            Eigen::VectorXd b(row_count);
            Eigen::VectorXd weights(row_count);
            Eigen::VectorXd correction(column_count);
            double previous_step = std::numeric_limits<double>::infinity();
            bool irls_solved = false;
            if (analyzed)
            {
                for (unsigned int iteration = 0;
                     iteration < max_irls_iterations; ++iteration)
                {
                    const double progress = max_irls_iterations > 1U
                        ? static_cast<double>(iteration) /
                              static_cast<double>(max_irls_iterations - 1U)
                        : 1.0;
                    const double sigma = initial_sigma * std::pow(
                        final_sigma / initial_sigma, progress);
                    const double sigma_sq = sigma * sigma;
                    for (Eigen::Index edge_index = 0;
                         edge_index < static_cast<Eigen::Index>(irls_edges.size());
                         ++edge_index)
                    {
                        const IrlsEdge &edge =
                            irls_edges[static_cast<size_t>(edge_index)];
                        const Matrix3d &ri = rotations.at(edge.i);
                        const Matrix3d &rj = rotations.at(edge.j);
                        const Matrix3d error =
                            rj.transpose() * edge.rotation * ri;
                        Eigen::AngleAxisd angle_axis(error);
                        const Vector3d tangent =
                            angle_axis.angle() * angle_axis.axis();
                        b.segment<3>(3 * edge_index) = tangent;
                        const double norm_sq = tangent.squaredNorm();
                        const double denominator = norm_sq + sigma_sq;
                        const double robust_weight =
                            robust_loss == "cauchy"
                                ? sigma_sq / denominator
                                : sigma_sq /
                                      (denominator * denominator);
                        weights.segment<3>(3 * edge_index).setConstant(
                            edge.base_weight * robust_weight);
                    }
                    const SparseMatrix weighted_mapping =
                        mapping.transpose() * weights.asDiagonal();
                    SparseMatrix normal = weighted_mapping * mapping;
                    // A component connected to Core is full rank in the
                    // intended graph.  The tiny diagonal is only a numerical
                    // guard for an accidentally duplicated/zero-weight edge;
                    // it does not affect scene-scale rotations.
                    normal.diagonal().array() += 1.0e-12;
                    linear_solver.factorize(normal);
                    if (linear_solver.info() != Eigen::Success)
                        break;
                    correction = linear_solver.solve(
                        weighted_mapping * b);
                    if (linear_solver.info() != Eigen::Success ||
                        !correction.allFinite())
                        break;
                    irls_solved = true;

                    double max_step = 0.0;
                    for (size_t index = 0; index < frontier_views.size(); ++index)
                    {
                        const Vector3d delta = correction.segment<3>(
                            static_cast<Eigen::Index>(3U * index));
                        max_step = std::max(max_step, delta.norm());
                        const double delta_norm = delta.norm();
                        if (delta_norm > 1.0e-12)
                        {
                            rotations[frontier_views[index]] =
                                rotations[frontier_views[index]] *
                                Eigen::AngleAxisd(
                                    delta_norm, delta / delta_norm)
                                    .toRotationMatrix();
                        }
                    }
                    if (max_step < 1.0e-8 ||
                        std::abs(previous_step - max_step) < 1.0e-10)
                        break;
                    previous_step = max_step;
                }
            }
            if (!irls_solved)
            {
                LOG_ERROR_EN << "[MethodRotationAveraging] fixed-Core Frontier IRLS "
                             << "failed to factorize or solve the anchored graph";
                LOG_ERROR_ZH << "[MethodRotationAveraging] 固定Core Frontier IRLS无法分解或求解锚定图";
                return false;
            }
        }
        }
        else
        {
            // Legacy fixed-Core update remains the default until the
            // scored-backbone pre-solve has passed scene-level A/B checks.
            for (unsigned int iteration = 0; iteration < 20U; ++iteration)
            {
                std::map<ViewId, Matrix3d> next = rotations;
                for (const auto &[view, neighbors] : adjacency)
                {
                    if (core_set.count(view) != 0U ||
                        protected_filtered_views.count(view) != 0U ||
                        neighbors.empty())
                        continue;
                    Eigen::Quaterniond reference;
                    bool have_reference = false;
                    Eigen::Vector4d accumulator = Eigen::Vector4d::Zero();
                    double total_weight = 0.0;
                    for (const auto &edge : neighbors)
                    {
                        const auto found = rotations.find(edge.neighbor);
                        if (found == rotations.end())
                            continue;
                        const Matrix3d expected = edge.transform * found->second;
                        if (!expected.allFinite())
                            continue;
                        Eigen::Quaterniond q(expected);
                        q.normalize();
                        if (!have_reference)
                        {
                            reference = q;
                            have_reference = true;
                        }
                        if (reference.dot(q) < 0.0)
                            q.coeffs() *= -1.0;
                        const double residual = angle_deg(
                            q.toRotationMatrix().transpose() *
                            rotations.at(view));
                        const double weight = edge.weight /
                            (1.0 + (residual / 2.0) * (residual / 2.0));
                        accumulator += weight * q.coeffs();
                        total_weight += weight;
                    }
                    if (have_reference && total_weight > 0.0 &&
                        accumulator.norm() > 1.0e-12)
                    {
                        Eigen::Quaterniond averaged(accumulator[3], accumulator[0],
                                                     accumulator[1], accumulator[2]);
                        averaged.normalize();
                        next[view] = averaged.toRotationMatrix();
                    }
                }
                rotations.swap(next);
            }
        }

        std::size_t indirect_jump_reverted = 0U;
        double maximum_indirect_jump_deg = 0.0;
        for (const ViewId view : indirect_filtered_prior_views)
        {
            const auto solved = rotations.find(view);
            if (solved == rotations.end())
                continue;
            const double jump_deg = angle_deg(
                prior.GetRotation(view).transpose() * solved->second);
            maximum_indirect_jump_deg = std::max(maximum_indirect_jump_deg,
                                                 jump_deg);
            if (jump_deg > filtered_prior_conflict_deg)
            {
                // With no direct Core edge, the retained graph provides no
                // Core residual improvement for this individual view. A
                // large indirect jump cannot certify a better Core gauge.
                solved->second = prior.GetRotation(view);
                ++indirect_jump_reverted;
            }
        }
        if (protect_filtered_prior)
            LOG_INFO_ALL << "RA_CORE_FIXED_FILTERED_PRIOR_GUARD_RESULT"
                         << " indirect_jump_reverted=" << indirect_jump_reverted
                         << " max_indirect_jump_deg=" << maximum_indirect_jump_deg;

        std::size_t changed_frontier = 0U;
        for (const auto &[view, rotation] : rotations)
        {
            if (core_set.count(view) != 0U || view >= output.Size())
                continue;
            if (!stable_r2::contract::IsProperRotation(rotation))
                return false;
            const Matrix3d before = output.GetRotation(view);
            const Vector3d prior_translation = output.GetTranslation(view);
            output.SetRotation(view, rotation);
            if (output.GetPoseFormat() == PoseFormat::RwTc &&
                prior_translation.allFinite() && before.allFinite())
            {
                const Vector3d center = -before.transpose() * prior_translation;
                output.SetTranslation(view, -rotation * center);
            }
            if ((before - rotation).norm() > 1.0e-10)
                ++changed_frontier;
            // Rotation averaging alone is not a pose certificate.  Preserve
            // the existing lifecycle/capability state verbatim; expansion
            // decides whether this rotation can support a metric transaction.
        }

        if (!update_certificate)
            return true;

        output_quality = prior_quality;
        output_quality.source_track_count = tracks.size();
        output_quality.source_observation_count = 0U;
        for (const auto &track : tracks)
            output_quality.source_observation_count += track.size();
        output_quality.source_track_fingerprint =
            types::PoseGraphTrackSnapshotFingerprint(tracks);
        // Fresh may re-index tracks.  Old index-aligned quality rows cannot
        // be reused in the new snapshot, even if Core support was validated.
        output_quality.tracks.clear();
        output_quality.tracks.resize(tracks.size());
        // The scored RA-2 graph has a different edge indexing/weighting from
        // the prior graph.  Rebuild edge quality in expansion from the new
        // RelativeRotations instead of carrying stale per-edge rows.
        output_quality.rotation_edges.clear();
        const std::set<ViewId> certified_core(core_views.begin(), core_views.end());
        output_quality.stable_r2_core_support_fingerprint =
            stable_r2::contract::CoreTrackSupportFingerprint(
                tracks, certified_core);
        output_quality.independent_core_support_fingerprint =
            output_quality.stable_r2_core_support_fingerprint;
        output_quality.stable_r2_core_support_keys =
            stable_r2::contract::CoreTrackSupportKeys(tracks, certified_core);
        output_quality.independent_core_support_keys =
            output_quality.stable_r2_core_support_keys;
        output_quality.stable_r2_core_feature_keys =
            stable_r2::contract::CoreTrackFeatureKeys(tracks, certified_core);
        output_quality.independent_core_feature_keys =
            output_quality.stable_r2_core_feature_keys;
        output_quality.source_pose_count = output.Size();
        output_quality.source_pose_fingerprint =
            types::PoseGraphGlobalPoseSnapshotFingerprint(output);
        output_quality.stable_r2_core_pose_fingerprint =
            stable_r2::contract::CorePoseFingerprint(output, core_views);
        output_quality.independent_core_pose_fingerprint =
            output_quality.stable_r2_core_pose_fingerprint;
        const RelativeRotations filtered_rotations =
            RotationOnlyCopy(filtered_relative_poses);
        output_quality.source_rotation_graph_fingerprint =
            stable_r2::contract::RotationPriorFingerprint(filtered_rotations);
        output_quality.stable_r2_generation =
            std::max<std::uint64_t>(1U, output_quality.stable_r2_generation + 1U);
        output_quality.independent_generation =
            std::max<std::uint64_t>(1U, output_quality.independent_generation + 1U);
        if (output_quality.views.size() < output.Size())
            output_quality.views.resize(output.Size());
        for (ViewId view = static_cast<ViewId>(prior.Size());
             view < output.Size(); ++view)
        {
            output_quality.views[view].role = PoseGraphViewRole::FRONTIER;
            output_quality.views[view].translation_certified = false;
        }
        LOG_INFO_ALL << "RA_CORE_FIXED_MODE enabled=1 fixed_views="
                     << core_views.size() << " updated_frontier="
                     << changed_frontier
                     << " anchor=" << anchor
                     << " source_rotation_graph_fingerprint="
                     << output_quality.source_rotation_graph_fingerprint;
        return true;
    }

    DataPtr MethodRotationAveraging::Run()
    {
        try
        {
            DisplayConfigInfo();

            // Get input data | 获取输入数据
            auto relative_poses_ptr = GetDataPtr<RelativePoses>(required_package_["data_relative_poses"]);
            if (!relative_poses_ptr)
            {
                LOG_ERROR_ZH << "[MethodRotationAveraging] 无相对位姿数据" << std::endl;
                LOG_ERROR_EN << "[MethodRotationAveraging] No relative poses data" << std::endl;
                return nullptr;
            }

            // Graph gates mutate RelativePoses, therefore every RA pass works
            // on a private copy and publishes the exact filtered graph.
            const DataPtr filtered_relative_data =
                required_package_["data_relative_poses"]->CopyData();
            auto filtered_relative_poses =
                GetDataPtr<RelativePoses>(filtered_relative_data);
            if (!filtered_relative_data || !filtered_relative_poses)
            {
                LOG_ERROR_EN << "[MethodRotationAveraging] Failed to clone relative poses";
                LOG_ERROR_ZH << "[MethodRotationAveraging] 无法复制相对位姿";
                return nullptr;
            }
            if (relative_poses_ptr->HasAttachmentReservoir())
            {
                RelativePoses::PoseVector reservoir(
                    relative_poses_ptr->GetAttachmentReservoir().begin(),
                    relative_poses_ptr->GetAttachmentReservoir().end());
                filtered_relative_poses->AssignAttachmentReservoir(
                    std::move(reservoir));
            }
            const bool fixed_core_mode = GetOptionAsBool(
                "core_view_fixed_mode", false);
            const bool score_from_prior = GetOptionAsBool(
                "rotation_graph_score_from_prior_rotations", false);
            if (score_from_prior && !fixed_core_mode)
            {
                LOG_ERROR_EN << "[MethodRotationAveraging] prior rotation graph scoring requires core_view_fixed_mode";
                return nullptr;
            }
            const auto prior_global = fixed_core_mode
                ? GetDataPtr<GlobalPoses>(GetPriorInfo("data_global_poses"))
                : nullptr;
            const auto prior_quality = fixed_core_mode
                ? GetDataPtr<types::PoseGraphQuality>(
                      GetPriorInfo("data_pose_graph_quality"))
                : nullptr;
            const auto prior_tracks = fixed_core_mode
                ? GetDataPtr<Tracks>(GetPriorInfo("data_tracks"))
                : nullptr;
            const auto prior_graph = fixed_core_mode
                ? GetDataPtr<RelativePoses>(
                      GetPriorInfo("data_relative_poses"))
                : nullptr;
            const auto prior_rotations = fixed_core_mode
                ? GetDataPtr<RelativeRotations>(
                      GetPriorInfo("data_relative_rotations"))
                : nullptr;
            if (fixed_core_mode)
            {
                if (!prior_global || !prior_quality || !prior_tracks ||
                    (!prior_graph && !prior_rotations))
                {
                    LOG_ERROR_EN << "[MethodRotationAveraging] core_view_fixed_mode requires GP, PoseGraphQuality, Tracks and prior filtered RelativePoses";
                    return nullptr;
                }
                const auto certificate =
                    stable_r2::contract::ValidateCertificate(
                        *prior_quality, *prior_tracks, *prior_global, true);
                if (!certificate.accepted)
                {
                    LOG_ERROR_EN << "[MethodRotationAveraging] refusing stale Core certificate: "
                                 << certificate.failure_reason;
                    return nullptr;
                }
                const std::uint64_t prior_graph_fingerprint = prior_graph
                    ? stable_r2::contract::RotationPriorFingerprint(
                          RotationOnlyCopy(*prior_graph))
                    : stable_r2::contract::RotationPriorFingerprint(
                          *prior_rotations);
                if (prior_quality->source_rotation_graph_fingerprint !=
                    prior_graph_fingerprint)
                {
                    LOG_ERROR_EN << "[MethodRotationAveraging] prior rotation graph fingerprint mismatch";
                    return nullptr;
                }
            }
            bool prior_scoring_ready = score_from_prior && prior_global;
            if (prior_scoring_ready)
            {
                const auto is_proper_rotation = [](const Matrix3d &rotation) {
                    return rotation.allFinite() &&
                        rotation.determinant() > 0.0 &&
                        (rotation.transpose() * rotation - Matrix3d::Identity())
                            .norm() < 1.0e-3;
                };
                for (const auto &pose : *filtered_relative_poses)
                {
                    const ViewId i = pose.GetViewIdI();
                    const ViewId j = pose.GetViewIdJ();
                    if (i >= prior_global->Size() || j >= prior_global->Size() ||
                        !is_proper_rotation(prior_global->GetRotation(i)) ||
                        !is_proper_rotation(prior_global->GetRotation(j)))
                    {
                        prior_scoring_ready = false;
                        break;
                    }
                }
                if (!prior_scoring_ready)
                {
                    LOG_WARNING_EN << "[MethodRotationAveraging] prior rotation snapshot does not cover every scored edge; disabling prior scoring and solver bypass";
                }
            }
            // In fixed-Core RA-2, a prior-only score is inexpensive but can
            // be stale on Frontier views.  The opt-in Chatterjee path first
            // solves the current graph with immutable Core boundary
            // conditions, then uses those solved rotations for the scored
            // backbone residuals.  Keep the legacy prior scorer untouched by
            // default until scene-level edge/accuracy A/B is complete.
            GlobalPoses fixed_core_scoring_prior;
            bool fixed_core_scoring_prior_ready = false;
            if (fixed_core_mode && prior_scoring_ready &&
                GetOptionAsBool("rotation_graph_fixed_core_irls", false))
            {
                types::PoseGraphQuality prepass_quality;
                fixed_core_scoring_prior = *prior_global;
                fixed_core_scoring_prior_ready = ApplyFixedCoreCommit(
                    *prior_global, *prior_global, *prior_quality, *prior_tracks,
                    *filtered_relative_poses, fixed_core_scoring_prior,
                    prepass_quality, false);
                if (!fixed_core_scoring_prior_ready)
                {
                    LOG_ERROR_EN << "[MethodRotationAveraging] fixed-Core IRLS prepass failed";
                    LOG_ERROR_ZH << "[MethodRotationAveraging] 固定Core IRLS预求解失败";
                    return nullptr;
                }
                LOG_INFO_ALL << "[MethodRotationAveraging] fixed-Core IRLS prepass "
                                "ready for scored-backbone residuals";
            }
            RelativePoses::RotationGraphFilterResult graph_filter_result{};
            std::string graph_filter_mode;
            if (!ApplyRotationGraphFilter(*filtered_relative_poses,
                                           graph_filter_result,
                                           graph_filter_mode,
                                           fixed_core_scoring_prior_ready
                                               ? &fixed_core_scoring_prior
                                               : (prior_scoring_ready
                                                      ? prior_global.get()
                                                      : nullptr)))
                return nullptr;

            // Create global pose data as output | 创建全局位姿数据作为输出
            auto global_poses_data = FactoryData::Create("data_global_poses");
            if (!global_poses_data)
            {
                LOG_ERROR_ZH << "[MethodRotationAveraging] 创建全局位姿数据失败" << std::endl;
                LOG_ERROR_EN << "[MethodRotationAveraging] Failed to create global poses data" << std::endl;
                return nullptr;
            }
            auto global_poses_ptr = GetDataPtr<GlobalPoses>(global_poses_data);
            if (!global_poses_ptr)
            {
                LOG_ERROR_ZH << "[MethodRotationAveraging] 获取GlobalPoses指针失败" << std::endl;
                LOG_ERROR_EN << "[MethodRotationAveraging] Failed to get GlobalPoses pointer" << std::endl;
                return nullptr;
            }

            // Set temporary file paths | 设置临时文件路径
            std::string temp_dir = GetOptionAsString("temp_dir", "./temp");
            std::filesystem::create_directories(temp_dir);

            temp_g2o_path_ = temp_dir + "/" + GetOptionAsString("g2o_filename", "relative_poses.g2o");
            temp_result_path_ = temp_dir + "/" + GetOptionAsString("estimator_output_g2o", "optimized_poses.g2o");

            std::string rotation_estimator = GetOptionAsString("rotation_estimator", "Chatterjee");

            bool success = false;
            if (fixed_core_mode && prior_scoring_ready)
            {
                // The fixed-Core commit performs the only required frontier
                // update.  Re-running Chatterjee on the full graph would
                // rebuild the dense synchronization problem that the prior
                // rotation snapshot was supplied specifically to avoid.
                *global_poses_ptr = fixed_core_scoring_prior_ready
                    ? fixed_core_scoring_prior : *prior_global;
                ViewId maximum_graph_view = 0;
                bool graph_has_views = false;
                for (const auto &pose : *filtered_relative_poses)
                {
                    maximum_graph_view = std::max(
                        maximum_graph_view,
                        std::max(pose.GetViewIdI(), pose.GetViewIdJ()));
                    graph_has_views = true;
                }
                if (graph_has_views)
                    global_poses_ptr->EnsureViewCapacity(
                        static_cast<std::size_t>(maximum_graph_view) + 1U);
                success = true;
                LOG_INFO_ALL << "[MethodRotationAveraging] fixed-Core prior "
                                "rotation fast path skipped full RA solver";
            }
            else if (rotation_estimator == "GraphOptim")
            {
                graphoptim_bin_folder_ = DetectGraphOptimBinPath();
                if (graphoptim_bin_folder_.empty())
                {
                    LOG_ERROR_ZH << "[MethodRotationAveraging] 未找到GraphOptim。请安装带GraphOptim依赖的插件包，或在Configuration中设置GraphOptim Bin Folder" << std::endl;
                    LOG_ERROR_EN << "[MethodRotationAveraging] GraphOptim was not found. Install the package dependency or set GraphOptim Bin Folder in Configuration" << std::endl;
                    return nullptr;
                }
                success = RunGraphOptim(*filtered_relative_poses, *global_poses_ptr);
            }
            else if (rotation_estimator == "Chatterjee")
            {
                // Create Chatterjee rotation averaging method instance via factory | 通过工厂创建Chatterjee旋转平均方法实例
                const std::string chatterjee_method_id = GetOptionAsString(
                    "chatterjee_method", "method_rotation_averaging_chatterjee");
                auto chatterjee_method = std::dynamic_pointer_cast<Interface::MethodPreset>(
                    FactoryMethod::Create(chatterjee_method_id));
                if (!chatterjee_method)
                {
                    LOG_ERROR_ZH << "[MethodRotationAveraging] 创建" << chatterjee_method_id << "失败" << std::endl;
                    LOG_ERROR_EN << "[MethodRotationAveraging] Failed to create " << chatterjee_method_id << std::endl;
                    return nullptr;
                }
                // Forward shared solver options to the child plugin.  The
                // child ignores wrapper-only graph-gate keys.
                chatterjee_method->SetMethodOptions(GetMethodOptions());

                // Execute Chatterjee method | 执行Chatterjee方法
                auto chatterjee_output_data = chatterjee_method->Build(filtered_relative_data);
                if (!chatterjee_output_data)
                {
                    LOG_ERROR_ZH << "[MethodRotationAveraging] method_rotation_averaging_Chatterjee失败" << std::endl;
                    LOG_ERROR_EN << "[MethodRotationAveraging] method_rotation_averaging_Chatterjee failed" << std::endl;
                    return nullptr;
                }
                // Get result from Chatterjee method (assuming it is also GlobalPoses) | 获取Chatterjee方法的结果 (假设它也是GlobalPoses)
                auto chatterjee_global_poses_ptr = GetDataPtr<GlobalPoses>(chatterjee_output_data);
                if (!chatterjee_global_poses_ptr)
                {
                    LOG_ERROR_ZH << "[MethodRotationAveraging] 从Chatterjee输出获取GlobalPoses失败" << std::endl;
                    LOG_ERROR_EN << "[MethodRotationAveraging] Failed to get GlobalPoses from Chatterjee output" << std::endl;
                    return nullptr;
                }
                *global_poses_ptr = *chatterjee_global_poses_ptr; // Copy result to main output | 将结果复制到主输出
                success = true;
            }
            else
            {
                LOG_ERROR_ZH << "[MethodRotationAveraging] 未知旋转估计器: " << rotation_estimator << std::endl;
                LOG_ERROR_EN << "[MethodRotationAveraging] Unknown rotation estimator: " << rotation_estimator << std::endl;
                return nullptr;
            }

            if (!success)
            {
                LOG_ERROR_ZH << "[MethodRotationAveraging] 估计全局旋转失败" << std::endl;
                LOG_ERROR_EN << "[MethodRotationAveraging] Failed to estimate global rotations" << std::endl;
                return nullptr;
            }

            DataPtr quality_output_data;
            if (fixed_core_mode)
            {
                GlobalPoses fixed_output;
                types::PoseGraphQuality fixed_quality;
                if (!ApplyFixedCoreCommit(*global_poses_ptr, *prior_global,
                                           *prior_quality, *prior_tracks,
                                           *filtered_relative_poses,
                                           fixed_output, fixed_quality))
                    return nullptr;
                *global_poses_ptr = fixed_output;
                quality_output_data = FactoryData::Create(
                    "data_pose_graph_quality");
                auto quality_output = GetDataPtr<types::PoseGraphQuality>(
                    quality_output_data);
                if (!quality_output_data || !quality_output)
                    return nullptr;
                *quality_output = fixed_quality;
            }

            // ========== Print estimation status | 打印估计状态 ==========
            PrintEstimationStatus(*global_poses_ptr, *filtered_relative_poses);

            // Preserve the legacy single-data output for the historical
            // default configuration.  Multi-output packages are used only
            // when the new graph/fixed-Core paths are enabled.
            if (graph_filter_mode == "none" && !fixed_core_mode)
                return global_poses_data;

            const DataPtr filtered_rotation_data =
                FactoryData::Create("data_relative_rotations");
            auto filtered_rotations = GetDataPtr<RelativeRotations>(
                filtered_rotation_data);
            if (!filtered_rotation_data || !filtered_rotations)
                return nullptr;
            *filtered_rotations = RotationOnlyCopy(*filtered_relative_poses);
            auto output = std::make_shared<DataPackage>();
            output->AddData("data_global_poses", global_poses_data);
            output->AddData("data_relative_poses", filtered_relative_data);
            output->AddData("data_relative_rotations", filtered_rotation_data);
            if (quality_output_data)
                output->AddData("data_pose_graph_quality", quality_output_data);
            return output;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_ZH << "[MethodRotationAveraging] 错误: " << e.what() << std::endl;
            LOG_ERROR_EN << "[MethodRotationAveraging] Error: " << e.what() << std::endl;
            return nullptr;
        }
    }

    bool MethodRotationAveraging::RunGraphOptim(
        const RelativePoses &relative_poses,
        GlobalPoses &global_poses)
    {

        // 1. Export relative poses to g2o file using PoSDK::file::SaveToG2O | 1. 导出相对位姿到g2o文件 using PoSDK::file::SaveToG2O
        if (!file::SaveToG2O(temp_g2o_path_, relative_poses)) // Use PoSDK::file::SaveToG2O
        {
            LOG_ERROR_ZH << "[MethodRotationAveraging] 使用file::SaveToG2O导出g2o文件失败: " << temp_g2o_path_ << std::endl;
            LOG_ERROR_EN << "[MethodRotationAveraging] Failed to export g2o file using file::SaveToG2O: " << temp_g2o_path_ << std::endl;
            return false;
        }

        // 2. Build GraphOptim command | 2. 构建GraphOptim命令
        const std::string rotation_estimator_path =
            (std::filesystem::path(graphoptim_bin_folder_) /
             "rotation_estimator").string();

        if (!CheckGraphOptimTool(rotation_estimator_path))
        {
            LOG_ERROR_ZH << "[MethodRotationAveraging] 未找到或不可执行GraphOptim工具" << std::endl;
            LOG_ERROR_EN << "[MethodRotationAveraging] GraphOptim tool not found or not executable" << std::endl;
            return false;
        }

        // Build command line - pass input g2o and output g2o filenames | 构建命令行 - 传递输入g2o和输出g2o文件名
        std::error_code remove_error;
        std::filesystem::remove(temp_result_path_, remove_error);
        std::stringstream cmd;
        cmd << PoSDK::PluginRuntime::QuotePosixShellArgument(rotation_estimator_path)
            << " --g2o_filename="
            << PoSDK::PluginRuntime::QuotePosixShellArgument(temp_g2o_path_)
            << " --output_g2o_filename="
            << PoSDK::PluginRuntime::QuotePosixShellArgument(temp_result_path_);

        LOG_DEBUG_ZH << "[MethodRotationAveraging] 执行命令: " << cmd.str() << std::endl;
        LOG_DEBUG_EN << "[MethodRotationAveraging] Executing command: " << cmd.str() << std::endl;

        // 3. Execute GraphOptim with profiling | 3. 执行GraphOptim并进行性能分析
        int ret = 0;
        {
            PROFILER_START_AUTO(enable_profiling_);
            ret = POSDK_SYSTEM(cmd.str().c_str()); // PROFILER_SYSTEM
            PROFILER_END();

            // Print profiling statistics | 打印性能分析统计
            if (SHOULD_LOG(DEBUG))
            {
                PROFILER_PRINT_STATS(enable_profiling_); // 当前会话统计
            }
        }

        if (ret != 0)
        {
            LOG_ERROR_ZH << "[MethodRotationAveraging] GraphOptim执行失败" << std::endl;
            LOG_ERROR_EN << "[MethodRotationAveraging] GraphOptim execution failed" << std::endl;
            return false;
        }

        // 4. Read results | 4. 读取结果
        if (!file::LoadFromG2O(temp_result_path_, global_poses))
        {
            LOG_ERROR_ZH << "[MethodRotationAveraging] 从G2O文件导入结果失败: " << temp_result_path_ << std::endl;
            LOG_ERROR_EN << "[MethodRotationAveraging] Failed to import results from G2O file: " << temp_result_path_ << std::endl;
            return false;
        }

        // 4.5. Initialize EstInfo based on loaded poses | 4.5. 根据加载的位姿初始化EstInfo
        // LoadFromG2O only loads rotations/translations, EstInfo needs to be set manually
        // LoadFromG2O只加载旋转/平移，EstInfo需要手动设置
        {
            size_t num_poses = global_poses.Size();
            global_poses.GetEstInfo().Init(num_poses);

            // Collect valid view IDs from input relative poses | 从输入相对位姿收集有效视图ID
            std::set<ViewId> valid_view_ids;
            for (size_t i = 0; i < relative_poses.size(); ++i)
            {
                const auto *rel_pose = relative_poses[i];
                if (rel_pose)
                {
                    valid_view_ids.insert(rel_pose->GetViewIdI());
                    valid_view_ids.insert(rel_pose->GetViewIdJ());
                }
            }

            // Mark all views that exist in relative_poses as ESTIMATED | 将relative_poses中存在的所有视图标记为ESTIMATED
            for (ViewId vid : valid_view_ids)
            {
                if (vid < num_poses)
                {
                    // Check if rotation is valid (not identity for non-reference view) | 检查旋转是否有效
                    global_poses.AddEstimatedView(vid);
                }
            }

            LOG_DEBUG_ZH << "[MethodRotationAveraging] 初始化EstInfo: " << num_poses << " 个位姿, "
                         << global_poses.GetNumEstimatedViews() << " 个已估计视图";
            LOG_DEBUG_EN << "[MethodRotationAveraging] Initialized EstInfo: " << num_poses << " poses, "
                         << global_poses.GetNumEstimatedViews() << " estimated views";
        }

        // 5. Clean up temporary files | 5. 清理临时文件
        LOG_DEBUG_ZH << "[MethodRotationAveraging] 为检查保留临时文件: " << temp_g2o_path_ << " 和 " << temp_result_path_ << std::endl;
        LOG_DEBUG_EN << "[MethodRotationAveraging] Preserving temporary files for inspection: " << temp_g2o_path_ << " and " << temp_result_path_ << std::endl;

        // Always clean up temporary files, but log details | 总是清理临时文件，但用详细日志记录
        std::filesystem::remove(temp_g2o_path_);
        std::filesystem::remove(temp_result_path_);
        LOG_DEBUG_ZH << "[MethodRotationAveraging] 已清理临时文件." << std::endl;
        LOG_DEBUG_EN << "[MethodRotationAveraging] Cleaned up temporary files." << std::endl;

        return true;
    }

    std::string MethodRotationAveraging::DetectGraphOptimBinPath() const
    {
        // Explicit Configuration override has the highest priority. Keep the
        // legacy GraphOptim_bin key readable for existing projects.
        std::string configured = GetOptionAsPath("graphoptim_bin_folder", "");
        if (configured.empty())
        {
            configured = GetOptionAsPath("GraphOptim_bin", "");
        }
        if (!configured.empty() &&
            CheckGraphOptimTool((std::filesystem::path(configured) /
                                 "rotation_estimator").string()))
        {
            return std::filesystem::canonical(configured).string();
        }

        // Signed Marketplace installation state. The stable binding survives
        // package upgrades; retain the versioned lookup for packages installed
        // by older PoSDK releases.
        const auto stable_installed =
            PoSDK::PluginRuntime::FindStableInstalledExecutable(
                "method_rotation_averaging", "graphoptim_bin",
                "rotation_estimator");
        if (!stable_installed.empty())
        {
            LOG_DEBUG_ZH << "[MethodRotationAveraging] 使用固定插件路径中的GraphOptim: "
                         << stable_installed.string() << std::endl;
            LOG_DEBUG_EN << "[MethodRotationAveraging] Using GraphOptim from the stable plugin path: "
                         << stable_installed.string() << std::endl;
            return stable_installed.parent_path().string();
        }

        const auto installed = PoSDK::PluginRuntime::FindInstalledExecutable(
            "method_rotation_averaging", "0.1.0",
            PoSDK::PluginRuntime::CurrentTargetSystem(),
            PoSDK::PluginRuntime::CurrentTargetArchitecture(),
            "rotation_estimator");
        if (!installed.empty())
        {
            LOG_DEBUG_ZH << "[MethodRotationAveraging] 使用插件市场安装的GraphOptim: "
                         << installed.string() << std::endl;
            LOG_DEBUG_EN << "[MethodRotationAveraging] Using Marketplace-installed GraphOptim: "
                         << installed.string() << std::endl;
            return installed.parent_path().string();
        }

        const auto path_binary = PoSDK::PluginRuntime::FindExecutableOnPath(
            "rotation_estimator");
        if (!path_binary.empty())
        {
            return path_binary.parent_path().string();
        }

        // Source-tree paths are compiled out of Consumer packages.
        const std::vector<std::string> candidate_paths =
            POSDK_PLUGIN_DEVELOPER_FALLBACKS(
            // Prioritize dependency under project source directory (recommended) | 优先尝试项目源码目录下的依赖（推荐方式）
            std::string(PROJECT_SOURCE_DIR) + "/dependencies/GraphOptim/build_scripted/bin",
            std::string(PROJECT_SOURCE_DIR) + "/dependencies/GraphOptim/bin",
            // Try upper level directory dependency (compatible with old structure) | 尝试上一级目录的依赖（兼容旧结构）
            std::string(PROJECT_SOURCE_DIR) + "/../dependencies/GraphOptim/build_scripted/bin",
            std::string(PROJECT_SOURCE_DIR) + "/../dependencies/GraphOptim/bin",
            std::string(PROJECT_SOURCE_DIR) + "/../dependencies/GraphOptim/build/bin",
            // Relative path alternatives (runtime paths) | 相对路径备选（运行时路径）
            "../../dependencies/GraphOptim/build_scripted/bin",
            "../../dependencies/GraphOptim/bin",
            "../dependencies/GraphOptim/build_scripted/bin",
            "../dependencies/GraphOptim/bin");

        for (const auto &path : candidate_paths)
        {
            if (path.empty())
                continue;

            std::string rotation_estimator_path = path + "/rotation_estimator";
            if (CheckGraphOptimTool(rotation_estimator_path))
            {
                LOG_DEBUG_ZH << "[MethodRotationAveraging] 在 " << path << " 找到GraphOptim" << std::endl;
                LOG_DEBUG_EN << "[MethodRotationAveraging] Found GraphOptim at: " << path << std::endl;
                return path;
            }
        }

        return "";
    }

    bool MethodRotationAveraging::CheckGraphOptimTool(const std::string &tool_path) const
    {
        const std::filesystem::path candidate(tool_path);
        if (candidate.has_parent_path())
        {
            std::error_code error;
            return std::filesystem::is_regular_file(candidate, error) && !error
#if defined(__APPLE__) || defined(__linux__)
                && ::access(candidate.c_str(), X_OK) == 0
#endif
                ;
        }
        return !PoSDK::PluginRuntime::FindExecutableOnPath(tool_path).empty();
    }

    void MethodRotationAveraging::PrintEstimationStatus(
        const GlobalPoses &global_poses,
        const RelativePoses &relative_poses) const
    {
        // Collect all view IDs from relative poses | 从相对位姿收集所有视图ID
        std::set<ViewId> input_view_ids;
        for (size_t i = 0; i < relative_poses.size(); ++i)
        {
            const auto *rel_pose = relative_poses[i];
            if (rel_pose)
            {
                input_view_ids.insert(rel_pose->GetViewIdI());
                input_view_ids.insert(rel_pose->GetViewIdJ());
            }
        }

        // Get estimation statistics | 获取估计统计信息
        size_t total_size = global_poses.Size();
        size_t num_estimated = global_poses.GetNumEstimatedViews();
        auto estimated_views = global_poses.GetViewsInState(EstInfo::ViewState::ESTIMATED);
        auto invalid_views = global_poses.GetViewsInState(EstInfo::ViewState::INVALID);
        auto valid_views = global_poses.GetViewsInState(EstInfo::ViewState::VALID);

        // Print summary table | 打印摘要表格
        std::cout << "\n";
        std::cout << "================================================================================\n";
        std::cout << ">>> Rotation Averaging Estimation Status | 旋转平均估计状态 <<<\n";
        std::cout << "================================================================================\n";

        std::cout << std::left << std::setw(35) << "Check Type | 检查类型"
                  << std::setw(15) << "Count | 数量"
                  << "Details | 详情\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        std::cout << std::setw(35) << "Input ViewPairs 输入视图对"
                  << std::setw(15) << relative_poses.size()
                  << "\n";

        std::cout << std::setw(35) << "Input Views 输入视图数"
                  << std::setw(15) << input_view_ids.size()
                  << "\n";

        std::cout << std::setw(35) << "Output GlobalPoses 输出位姿数"
                  << std::setw(15) << total_size
                  << "\n";

        std::cout << std::setw(35) << "Estimated Views 已估计视图"
                  << std::setw(15) << num_estimated;
        if (num_estimated == input_view_ids.size())
        {
            std::cout << "✓ All views estimated | 所有视图已估计";
        }
        else
        {
            std::cout << "⚠ " << (input_view_ids.size() - num_estimated) << " views not estimated | 视图未估计";
        }
        std::cout << "\n";

        // Print estimated view IDs (sample if too many) | 打印已估计视图ID（如果太多则抽样）
        std::cout << "\n--- Estimated View IDs | 已估计的视图ID ---\n";
        if (estimated_views.empty())
        {
            std::cout << "  (none) | (无)\n";
        }
        else
        {
            std::cout << "  Total: " << estimated_views.size() << " views | 共 " << estimated_views.size() << " 个视图\n";
            std::cout << "  IDs: ";
            size_t display_count = std::min(estimated_views.size(), size_t(20));
            for (size_t i = 0; i < display_count; ++i)
            {
                std::cout << estimated_views[i];
                if (i < display_count - 1)
                    std::cout << ", ";
            }
            if (estimated_views.size() > display_count)
            {
                std::cout << " ... (" << (estimated_views.size() - display_count) << " more)";
            }
            std::cout << "\n";
        }

        // Print invalid view IDs if any | 打印无效视图ID（如果有）
        if (!invalid_views.empty())
        {
            std::cout << "\n--- Invalid View IDs | 无效的视图ID ---\n";
            std::cout << "  ⚠ Total: " << invalid_views.size() << " views | 共 " << invalid_views.size() << " 个视图\n";
            std::cout << "  IDs: ";
            size_t display_count = std::min(invalid_views.size(), size_t(20));
            for (size_t i = 0; i < display_count; ++i)
            {
                std::cout << invalid_views[i];
                if (i < display_count - 1)
                    std::cout << ", ";
            }
            if (invalid_views.size() > display_count)
            {
                std::cout << " ... (" << (invalid_views.size() - display_count) << " more)";
            }
            std::cout << "\n";
        }

        // Print valid but not estimated view IDs if any | 打印有效但未估计的视图ID（如果有）
        if (!valid_views.empty())
        {
            std::cout << "\n--- Valid but Not Estimated View IDs | 有效但未估计的视图ID ---\n";
            std::cout << "  Total: " << valid_views.size() << " views | 共 " << valid_views.size() << " 个视图\n";
            std::cout << "  IDs: ";
            size_t display_count = std::min(valid_views.size(), size_t(20));
            for (size_t i = 0; i < display_count; ++i)
            {
                std::cout << valid_views[i];
                if (i < display_count - 1)
                    std::cout << ", ";
            }
            if (valid_views.size() > display_count)
            {
                std::cout << " ... (" << (valid_views.size() - display_count) << " more)";
            }
            std::cout << "\n";
        }

        // Check for missing views (in input but not in output) | 检查缺失视图（在输入中但不在输出中）
        std::vector<ViewId> missing_views;
        for (ViewId vid : input_view_ids)
        {
            if (!global_poses.IsEstimated(vid))
            {
                missing_views.push_back(vid);
            }
        }

        if (!missing_views.empty())
        {
            std::cout << "\n--- Missing Views (in input but not estimated) | 缺失视图（在输入中但未估计）---\n";
            std::cout << "  ⚠ Total: " << missing_views.size() << " views | 共 " << missing_views.size() << " 个视图\n";
            std::cout << "  IDs: ";
            size_t display_count = std::min(missing_views.size(), size_t(20));
            for (size_t i = 0; i < display_count; ++i)
            {
                std::cout << missing_views[i];
                if (i < display_count - 1)
                    std::cout << ", ";
            }
            if (missing_views.size() > display_count)
            {
                std::cout << " ... (" << (missing_views.size() - display_count) << " more)";
            }
            std::cout << "\n";
        }

        std::cout << "================================================================================\n";
        std::cout << std::endl;

        // Also log to bilingual system | 同时输出到双语日志系统
        LOG_INFO_ZH << "[MethodRotationAveraging] 旋转估计完成: 输入 " << input_view_ids.size()
                    << " 个视图, 成功估计 " << num_estimated << " 个视图";
        LOG_INFO_EN << "[MethodRotationAveraging] Rotation estimation completed: input " << input_view_ids.size()
                    << " views, successfully estimated " << num_estimated << " views";

        if (!missing_views.empty())
        {
            LOG_WARNING_ZH << "[MethodRotationAveraging] 警告: " << missing_views.size() << " 个输入视图未能估计旋转";
            LOG_WARNING_EN << "[MethodRotationAveraging] Warning: " << missing_views.size() << " input views failed to estimate rotation";
        }
    }

} // namespace PoSDK

// Register plugin | 注册插件
// ✅ 使用单参数模式，自动从 CMake 读取 PLUGIN_NAME（实现单一信息源）
REGISTRATION_PLUGIN(PoSDK::MethodRotationAveraging)
