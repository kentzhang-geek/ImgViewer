cmake_minimum_required(VERSION 3.23)

project(ImgViewer LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	# static build by default
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# ---- 源码根目录（新增这一层）----
set(SRC_ROOT "${CMAKE_SOURCE_DIR}")

# ---- 第三方路径 ----
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

# ---- 源文件 ----
set(PROJECT_SOURCES
	${SRC_ROOT}/pch.cpp
	${SRC_ROOT}/main.cpp
	${SRC_ROOT}/DX12Renderer.cpp
	${SRC_ROOT}/DX12Renderer.h
	${SRC_ROOT}/ImageViewer.cpp
	${SRC_ROOT}/ImageViewer.h
	${SRC_ROOT}/ImageViewerUI.cpp
	${SRC_ROOT}/ImageViewerUI.h
	${SRC_ROOT}/ImageRenderer.cpp
	${SRC_ROOT}/ImageRenderer.h
)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES} ${IMGUI_SOURCE})

# ---- PCH ----
target_precompile_headers(${PROJECT_NAME} PRIVATE ${SRC_ROOT}/pch.h)

# ---- 宏 ----
target_compile_definitions(${PROJECT_NAME} PRIVATE
	_WINDOWS
	UNICODE
	_UNICODE
	NOMINMAX
	NDEBUG
	_CRT_SECURE_NO_WARNINGS
)

# ---- include ----
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
)

# ---- lib dirs ----
target_link_directories(${PROJECT_NAME} PRIVATE
	"../SDKs/boost/lib"
	"../SDKs/flatbuffers/lib"
	"../SDKs/dxtex/lib"
)

# ---- link ----
target_link_libraries(${PROJECT_NAME} PRIVATE
	DirectXTex.lib
	dxcompiler.lib
	d3dcompiler.lib
	dxgi.lib
	dxguid.lib
	D3d12.lib
	dwmapi.lib
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3 /permissive-)
endif()
