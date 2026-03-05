#### TEST-PROJECT FOR MY OWN WRAPPER

#### before building:
- Vulkan SDK - required for shader compilation
- run `make build_shadercross` - to build DDL_shadercross tool from the submodule, which is needed to convert shaders to Metal (MSL) and DirectX (DXIL) formats
- The shader compilation step (`build_shaders.sh`) runs automatically as part of the CMake build, but it depends on shadercross being built first
