# Order-independent transparency demo

An application that implements a variety of Order-independent transparency (OIT) techniques.
The sample allows easy comparison of the results of the supported rendering algorithms.
It also exposes the shortcomings of each technique via a few tweakable parameters.

## Background

Transparency rendering is a technique used to render objects that partially let light go through them.
It is heavily used in games to represent glass, fluids (e.g. smoke) and elements that have no physical equivalent in reality (e.g. magic).
It is also often used in computer-aided design to let creators see through multiple layers of architectural and mechanical designs.

Transparency is typically achieved by alpha blending fragments from back to front from the camera's perspective.
Technically, this means sorting the geometry on the CPU before sending it to the GPU for rendering.
However, in addition to having a non-negligeable CPU cost, that solution cannot produce accurate results in several situations, e.g. intersecting geometry or meshes with concave hulls.

OIT is a family of GPU techniques that removes the need of sorting geometry before rendering.
Some algorithms produce accurate transparency rendering results, while others aim for fast approximations.
Each algorithm has advantages and disadvantages, trading off between quality, memory usage and performance.

## Usage

The sample consists of two independent views.
Each view displays the same model rendered with one of the supported OIT algorithms.
A configuration window is available to tweak the following parameters:

- Transparent model
    - Opacity in both views
    - Choice of mesh
    - Choice of vertex color or texture
    - Different triangle order (to make the algorithm issues more apparent)
- Display of an optional background mesh
- For each view
    - OIT algorithm
    - Options specific to the selected algorithm, mostly meant to expose shortcomings

The following metrics are displayed to compare the algorithms to each other:

- GPU frame time in milliseconds
- Quality
    - The results of an algorithm are compared to the most accurate algorithm available (Per-pixel linked list)
    - The quality score is between 0.0 (poorest quality) and 1.0 (best quality)

## Supported algorithms

| Algorithm                 | Type              | Additional options
| ---                       | ---               | ---
| Unsorted over             | Approximate       | Split draw calls for back/front faces
| Per-pixel linked lists    | Exact             | Control the size of the fragment buffer
| Depth peeling             | Exact             | Number of layers
| Weighted sum              | Approximate       |
| New blended operator      | Approximate       |

