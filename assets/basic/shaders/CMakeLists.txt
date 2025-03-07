# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
generate_rules_for_shader("shader_static_vertex_colors" SOURCE "${PPX_DIR}/assets/basic/shaders/StaticVertexColors.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_vertex_colors" SOURCE "${PPX_DIR}/assets/basic/shaders/VertexColors.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_vertex_colors_push_constants" SOURCE "${PPX_DIR}/assets/basic/shaders/VertexColorsPushConstants.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_vertex_layout_test" SOURCE "${PPX_DIR}/assets/basic/shaders/VertexLayoutTest.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_texture" SOURCE "${PPX_DIR}/assets/basic/shaders/Texture.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_compute_fill" SOURCE "${PPX_DIR}/assets/basic/shaders/ComputeFill.hlsl" STAGES "cs")
generate_rules_for_shader("shader_cubemap" SOURCE "${PPX_DIR}/assets/basic/shaders/CubeMap.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_skybox" SOURCE "${PPX_DIR}/assets/basic/shaders/SkyBox.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_depth" SOURCE "${PPX_DIR}/assets/basic/shaders/Depth.hlsl" STAGES "vs")
generate_rules_for_shader("shader_diffuse_shadow" SOURCE "${PPX_DIR}/assets/basic/shaders/DiffuseShadow.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_normal_map" SOURCE "${PPX_DIR}/assets/basic/shaders/NormalMap.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_pbr_metallic_roughness" SOURCE "${PPX_DIR}/assets/basic/shaders/PbrMetallicRoughness.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_fullscreen_triangle" SOURCE "${PPX_DIR}/assets/basic/shaders/FullScreenTriangle.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_text_draw" SOURCE "${PPX_DIR}/assets/basic/shaders/TextDraw.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_image_filter" SOURCE "${PPX_DIR}/assets/basic/shaders/ImageFilter.hlsl" STAGES "cs")
generate_rules_for_shader("shader_static_texture" SOURCE "${PPX_DIR}/assets/basic/shaders/StaticTexture.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_texture_mip" SOURCE "${PPX_DIR}/assets/basic/shaders/TextureMip.hlsl" STAGES "vs" "ps")
generate_rules_for_shader("shader_passthrough_pos" SOURCE "${PPX_DIR}/assets/basic/shaders/PassThroughPos.hlsl" STAGES "vs")
generate_rules_for_shader("shader_unlit" SOURCE "${PPX_DIR}/assets/basic/shaders/Unlit.hlsl" STAGES "ps")
generate_rules_for_shader("shader_push_constants_texture" SOURCE "${PPX_DIR}/assets/basic/shaders/PushConstantsTexture.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_push_descriptors_buffers_texture" SOURCE "${PPX_DIR}/assets/basic/shaders/PushDescriptorsBuffersTexture.hlsl" STAGES "ps" "vs")
generate_rules_for_shader("shader_push_descriptors_texture" SOURCE "${PPX_DIR}/assets/basic/shaders/PushDescriptorsTexture.hlsl" STAGES "ps" "vs")