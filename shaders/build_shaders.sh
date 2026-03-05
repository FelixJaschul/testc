#!/usr/bin/env bash
set -euo pipefail

SHADER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SHADER_DIR}/.." && pwd)"
cd "$SHADER_DIR"

find_glslang() {
  if command -v glslangValidator >/dev/null 2>&1; then
    command -v glslangValidator
    return 0
  fi
  if [[ -n "${VULKAN_SDK:-}" ]] && [[ -x "${VULKAN_SDK}/bin/glslangValidator" ]]; then
    echo "${VULKAN_SDK}/bin/glslangValidator"
    return 0
  fi
  if [[ -x "$HOME/VulkanSDK/1.4.335.1/macOS/bin/glslangValidator" ]]; then
    echo "$HOME/VulkanSDK/1.4.335.1/macOS/bin/glslangValidator"
    return 0
  fi
  return 1
}

build_shadercross_from_submodule() {
  local local_bin="${ROOT_DIR}/.build/shadercross/shadercross"
  if [[ -x "${local_bin}" ]]; then
    echo "${local_bin}"
    return 0
  fi
  return 1
}

GLSLANG_BIN="$(find_glslang || true)"
if [[ -z "${GLSLANG_BIN}" ]]; then
  echo "glslangValidator not found. Install Vulkan SDK or set VULKAN_SDK." >&2
  exit 1
fi

echo "Compiling GLSL -> SPIR-V"
SHADERS=(
  "voxel_raymarch"
  "basic_triangle"
)

for shader in "${SHADERS[@]}"; do
  "${GLSLANG_BIN}" -V "${shader}.vert" -o "${shader}.vert.spv"
  "${GLSLANG_BIN}" -V "${shader}.frag" -o "${shader}.frag.spv"
done

SHADERCROSS_BIN=""
if command -v shadercross >/dev/null 2>&1; then
  SHADERCROSS_BIN="$(command -v shadercross)"
else
  SHADERCROSS_BIN="$(build_shadercross_from_submodule || true)"
fi

if [[ -n "${SHADERCROSS_BIN}" ]]; then
  echo "Converting SPIR-V -> MSL/DXIL via shadercross"
  for shader in "${SHADERS[@]}"; do
    "${SHADERCROSS_BIN}" "${shader}.vert.spv" -s SPIRV -d MSL  -t vertex   -e main -o "${shader}.vert.msl"
    "${SHADERCROSS_BIN}" "${shader}.frag.spv" -s SPIRV -d MSL  -t fragment -e main -o "${shader}.frag.msl"
    "${SHADERCROSS_BIN}" "${shader}.vert.spv" -s SPIRV -d DXIL -t vertex   -e main -o "${shader}.vert.dxil"
    "${SHADERCROSS_BIN}" "${shader}.frag.spv" -s SPIRV -d DXIL -t fragment -e main -o "${shader}.frag.dxil"
  done
else
  if command -v spirv-cross >/dev/null 2>&1; then
    echo "shadercross unavailable, using spirv-cross for MSL generation"
    for shader in "${SHADERS[@]}"; do
      spirv-cross "${shader}.vert.spv" --msl --output "${shader}.vert.msl"
      spirv-cross "${shader}.frag.spv" --msl --output "${shader}.frag.msl"
    done
  else
    echo "shadercross unavailable (not installed and no local build at .build/shadercross/shadercross); generated SPIR-V only."
  fi
fi

if [[ -f voxel_raymarch.vert.msl ]] && [[ -f voxel_raymarch.frag.msl ]] && command -v xcrun >/dev/null 2>&1; then
  if xcrun -sdk macosx -find metal >/dev/null 2>&1 && xcrun -sdk macosx -find metallib >/dev/null 2>&1; then
    echo "Compiling MSL -> metallib"
    for shader in "${SHADERS[@]}"; do
      if [[ -f "${shader}.vert.msl" ]]; then
        xcrun -sdk macosx metal -x metal -c "${shader}.vert.msl" -o "${shader}.vert.air"
        xcrun -sdk macosx metallib "${shader}.vert.air" -o "${shader}.vert.metallib"
      fi
      if [[ -f "${shader}.frag.msl" ]]; then
        xcrun -sdk macosx metal -x metal -c "${shader}.frag.msl" -o "${shader}.frag.air"
        xcrun -sdk macosx metallib "${shader}.frag.air" -o "${shader}.frag.metallib"
      fi
    done
  else
    echo "xcrun found but metal toolchain missing; skipping metallib generation."
  fi
fi

echo "Done"
