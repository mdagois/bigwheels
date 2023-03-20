# Copyright 2023 Google LLC
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

set(INCLUDE_FILES
    "${PPX_DIR}/assets/xls_viewer/shaders/Common.hlsli")

generate_rules_for_shader(
	"screen"
	SOURCE "${PPX_DIR}/assets/xls_viewer/shaders/Screen.hlsl"
    INCLUDES ${INCLUDE_FILES}
    STAGES "vs" "ps")

generate_group_rule_for_shader(
    "shader_xls_viewer"
    CHILDREN
    "screen"
)

