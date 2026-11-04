#pragma once
#include <po_core.hpp>
namespace PluginMethods {
class MethodRoMaV2 : public PoSDK::Interface::MethodPresetProfiler {
public:
        // <posdk:copyright>
        PoSDK::Interface::CopyrightData Copyright() const override
        {
            return {"PoSDK/method_romav2", "Copyright (c) 2026 qi Cai\nAuthor: qi Cai\nLicense: LicenseRef-Custom\nEmail: qicaiCN@gmail.com\n\n# Third-party notices\n\nPoSDK integration: qi Cai. Upstream algorithm authorship is preserved.\n\n<!-- posdk:upstream-research:v1:start -->\n## Original research and authors / 原论文与原作团队\n\n**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.\n\n### RoMa v2: Harder Better Faster Denser Feature Matching\n\n- **Authors / 原作者：** Johan Edstedt, David Nordström, Yushan Zhang, Georg Bökman, Jonathan Astermark, Viktor Larsson, Anders Heyden, Fredrik Kahl, Mårten Wadenbäck, Michael Felsberg.\n- **Team / 团队：** Linköping University; Chalmers University of Technology; University of Amsterdam; Lund University.\n- **Publication / 论文：** 2025 · arXiv preprint.\n- **Paper / 原文：** [RoMa v2: Harder Better Faster Denser Feature Matching](https://arxiv.org/abs/2511.15706).\n- **Official project or implementation / 原作项目或实现：** [https://github.com/Parskatt/RoMaV2](https://github.com/Parskatt/RoMaV2).\n\nThe matcher is the original pretrained **RoMa v2 v2.0.1**. DINOv3 is a separate Meta foundation-model dependency with its own license. MV-RoMa and learned PoGT adapters are not part of this plugin.\n\nAttribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.\n<!-- posdk:upstream-research:v1:end -->\n# Upstream provenance\n\nPoMVG integration maintainer: qi Cai. Upstream model authorship is preserved.\n\n- RoMa v2: Johan Edstedt and the coauthors in the upstream README;\n  https://github.com/Parskatt/RoMaV2 ; MIT (Copyright 2025 Johan Edstedt).\n  Revision `95c9968145c8906b7b59383258e9f73b02853d89`, model v2.0.1:\n  https://github.com/Parskatt/RoMaV2/releases/download/v2.0.1/romav2.0.1.pt .\n- DINOv3: Meta and upstream contributors;\n  https://github.com/facebookresearch/dinov3 ; revision\n  `adc254450203739c8149213a7a69d8d905b4fcfa`. The separate upstream\n  `LICENSE.md` applies; DINOv3 is not covered by RoMa's MIT license.\n- PyTorch, torchvision, NumPy, Pillow and einops retain all upstream licenses\n  and notices in their installed distributions. Runtime versions and source\n  and model hashes are recorded by each successful worker run.\n\nThis development plugin does not redistribute model weights or DINOv3. The\nexplicitly selected remote runtime supplies them. A future downloadable\npackage requires complete license texts and a locked isolated installer.\n"};
        }
        // </posdk:copyright>

    MethodRoMaV2();
    const std::string &GetType() const override;
    PoSDK::Interface::DataPtr Run() override;
};
}
