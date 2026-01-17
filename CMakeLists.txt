cmake_minimum_required(VERSION 3.23)

project(ImgViewer LANGUAGES CXX RC)
set(BINARY_NAME "imgViewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Source Root ----
set(SRC_ROOT "${CMAKE_SOURCE_DIR}")

# ---- Third-party Paths ----
set(SDK_ROOT         "../SDKs")

set(IMGUI_SOURCE
	${SDK_ROOT}/imgui-docking/imgui.cpp
	${SDK_ROOT}/imgui-docking/imgui_demo.cpp
	${SDK_ROOT}/imgui-docking/imgui_draw.cpp
	${SDK_ROOT}/imgui-docking/imgui_tables.cpp
	${SDK_ROOT}/imgui-docking/imgui_widgets.cpp
	${SDK_ROOT}/imgui-docking/backends/imgui_impl_dx12.cpp
	${SDK_ROOT}/imgui-docking/backends/imgui_impl_win32.cpp
)

# ---- Source Files ----
set(PROJECT_SOURCES
	${SRC_ROOT}/pch.cpp
	${SRC_ROOT}/main.cpp
	${SRC_ROOT}/DX12Renderer.cpp
	${SRC_ROOT}/DX12Renderer.h
	${SRC_ROOT}/ImgViewer.cpp
	${SRC_ROOT}/ImgViewer.h
	${SRC_ROOT}/ImgViewerUI.cpp
	${SRC_ROOT}/ImgViewerUI.h
	${SRC_ROOT}/ImageRenderer.cpp
	${SRC_ROOT}/ImageRenderer.h
)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES} ${IMGUI_SOURCE})

# ---- PCH ----
target_precompile_headers(${PROJECT_NAME} PRIVATE ${SRC_ROOT}/pch.h)

# ---- Macros ----
target_compile_definitions(${PROJECT_NAME} PRIVATE
	_WINDOWS
	UNICODE
	_UNICODE
	NOMINMAX
	NDEBUG
	_CRT_SECURE_NO_WARNINGS
)

# ---- Include Directories ----
target_include_directories(${PROJECT_NAME} PRIVATE
	"../scene_flat/generated/cpp"
	${SRC_ROOT}
	"../Common"
	${SDK_ROOT}/imgui-docking
	${SDK_ROOT}/imgui-docking/backends
	"../SDKs/flatbuffers/include"
	"../SDKs/boost/include/boost-1_89"
	"../SDKs/stb"
	"../SDKs/dxtex/include"
	"../SDKs/DirectXTex-oct2025/Common"
	"../SDKs/libjpeg/include"
)

# ---- Library Directories ----
target_link_directories(${PROJECT_NAME} PRIVATE
	"../SDKs/boost/lib"
	"../SDKs/flatbuffers/lib"
	"../SDKs/dxtex/lib"
	"../SDKs/libjpeg/lib"
)

# ---- Link Libraries ----
target_link_libraries(${PROJECT_NAME} PRIVATE
	DirectXTex.lib
	dxcompiler.lib
	d3dcompiler.lib
	dxgi.lib
	dxguid.lib
	D3d12.lib
	dwmapi.lib
	jpeg-static.lib
)

target_compile_options(${PROJECT_NAME} PRIVATE /W3 /permissive-)

# ---- Output Settings ----
set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME ${BINARY_NAME}
)
