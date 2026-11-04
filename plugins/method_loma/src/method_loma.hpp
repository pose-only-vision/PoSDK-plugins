#pragma once
#include <po_core.hpp>
namespace PluginMethods {
class MethodLoMa : public PoSDK::Interface::MethodPresetProfiler {
public:
        // <posdk:copyright>
        PoSDK::Interface::CopyrightData Copyright() const override
        {
            return {"PoSDK/method_loma", "Copyright (c) 2026 qi Cai\nAuthor: qi Cai\nLicense: LicenseRef-Custom\nEmail: qicaiCN@gmail.com\n\n# Third-party notices\n\nPoSDK integration: qi Cai. Upstream algorithm authorship is preserved.\n\n<!-- posdk:upstream-research:v1:start -->\n## Original research and authors / 原论文与原作团队\n\n**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.\n\n### LoMa: Local Feature Matching Revisited\n\n- **Authors / 原作者：** David Nordström, Johan Edstedt, Georg Bökman, Jonathan Astermark, Anders Heyden, Viktor Larsson, Mårten Wadenbäck, Michael Felsberg, Fredrik Kahl.\n- **Team / 团队：** Chalmers University of Technology; Linköping University; University of Amsterdam; Lund University.\n- **Publication / 论文：** 2026 · ECCV 2026.\n- **Paper / 原文：** [LoMa: Local Feature Matching Revisited](https://arxiv.org/abs/2604.04931).\n- **Official project or implementation / 原作项目或实现：** [https://github.com/davnords/LoMa](https://github.com/davnords/LoMa).\n\nThis plugin currently exposes **LoMa-B / DaD + DeDoDe-G**, using ONNX assets published by the original authors and a COLMAP-compatible adapter. It does not advertise LoMa-L, LoMa-G or MVLoMa support.\n\nAttribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.\n<!-- posdk:upstream-research:v1:end -->\n\n## Separate components and licenses\n\n- LoMa source and DaD/DeDoDe components: original authors; MIT except the\n  LightGlue-derived matcher, which retains Apache-2.0.\n- ONNX assets are retrieved from the original authors' davnords/storage\n  release and SHA256-verified; no model weights are embedded in the plugin.\n- COLMAP: BSD-3-Clause. The independent Python adapter follows the pinned\n  LoMa ONNX input/output and Bitmap resampling contract.\n- ONNX Runtime: Microsoft and contributors; MIT. CoreML: Apple platform framework.\n- Python, NumPy, Pillow, pycolmap and their dependencies retain their\n  individual distribution notices in the remote isolated environment.\n"};
        }
        // </posdk:copyright>

    MethodLoMa();
    const std::string &GetType() const override;
    PoSDK::Interface::DataPtr Run() override;
};
}
