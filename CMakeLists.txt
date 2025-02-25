cmake_minimum_required(VERSION 3.10)
project(EPaperDisplayWebApp LANGUAGES C CXX)

# 設定 C / C++ 標準
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 加入選項：是否啟用 7.3 吋電子紙支援
option(USE_7_3_EPAPER "Enable support for 7.3 inch E-Paper" OFF)
option(USE_4_0_EPAPER "Enable support for 4.0 inch E-Paper" OFF)

# 編譯選項
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2 -g -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2 -g -ffunction-sections -fdata-sections")

# 定義編譯參數
# 定義編譯參數：根據選項決定使用哪個電子紙版本
if(USE_7_3_EPAPER)
    message(STATUS "Building with support for 7.3 inch E-Paper")
    add_definitions(-DUSE_7_3_EPAPER)
elseif(USE_4_0_EPAPER)
    message(STATUS "Building with support for 4.0 inch E-Paper (default)")
    add_definitions(-DUSE_4_0_EPAPER)
else()
    message(STATUS "Building with support for 4.0 inch E-Paper (default)")
    add_definitions(-DUSE_4_0_EPAPER)
endif()

# 其他通用的編譯參數
add_definitions(-DRPI -DUSE_LGPIO_LIB -DDEBUG)

# 尋找依賴項
# find_package(OpenMP REQUIRED)
# find_package(Boost REQUIRED) 
find_package(OpenCV REQUIRED)

# 收集 lib 中的 C 檔案 (Config, e-Paper, Fonts, GUI)
file(GLOB CONFIG_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/Config/*.c")
file(GLOB EPAPER_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/e-Paper/*.c")
file(GLOB FONTS_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/Fonts/*.c")
file(GLOB GUI_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/GUI/*.c")

set(ALL_C_SOURCES
    ${CONFIG_C_SOURCES}
    ${EPAPER_C_SOURCES}
    ${FONTS_C_SOURCES}
    ${GUI_C_SOURCES}
)

# 建立電子紙靜態庫 epdlib
add_library(epdlib STATIC ${ALL_C_SOURCES})
target_include_directories(epdlib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/Config"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/e-Paper"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/Fonts"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/GUI"
)

# 建立網頁靜態庫 Mongoose 
file(GLOB MONGOOSE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/mongoose/*.c")
add_library(mongoose STATIC ${MONGOOSE_SOURCES})
target_include_directories(mongoose PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/mongoose")


# 搜尋 src 下所有 C++ 檔案 (main.cpp, epaper.cpp, image_processing.cpp, server.cpp)
file(GLOB CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# 建立執行檔 ePaper_web
add_executable(ePaper_web ${CPP_SOURCES})
target_include_directories(ePaper_web PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"         # 加入專案根目錄
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(ePaper_web
    PRIVATE
        # OpenMP::OpenMP_CXX
        epdlib
        mongoose
        pthread
        lgpio
        m
        ${OpenCV_LIBS}
)
