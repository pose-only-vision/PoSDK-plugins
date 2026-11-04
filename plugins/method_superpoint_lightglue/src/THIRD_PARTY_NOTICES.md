# Third-party notices

The wrapper integration is derived from the VINF-SJTU PoSDK ALIKEDLightGlue
frontend, maintained by qi Cai, under MPL-2.0. The wrapper contains no upstream
model implementation or weights. The signed installer retrieves pinned upstream
dependencies into a version-specific private runtime.

SuperPoint (Daniel DeTone, Tomasz Malisiewicz, Andrew Rabinovich; Magic Leap)
and its weights are subject to the upstream noncommercial research license:
https://github.com/magicleap/SuperPointPretrainedNetwork/blob/master/LICENSE
The full license is included as licenses/SuperPoint-LICENSE.txt. Installing the
plugin does not grant additional rights to SuperPoint. Users must have the
rights required by that license before installing or using the dependency.

LightGlue (Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys; ETH Zurich)
is Apache-2.0, except for separately licensed feature extractors.
Source commit: eb42fee2d71449efb0aa5c10549752b5d75384d8.
https://github.com/cvg/LightGlue

Python 3.11.11, PyTorch 2.2.2, torchvision 0.17.2, NumPy 1.26.4,
Kornia 0.6.12 and Pillow 12.3.0 retain their upstream licenses.
Exact Conda and wheel versions/hashes are in deps/*.lock. Verbatim wheel license
and notice snapshots are included under licenses/runtime/.
