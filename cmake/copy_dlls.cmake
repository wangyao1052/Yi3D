# copy_dlls.cmake
# 递归解析 EXE/DLL 的运行时依赖，自动拷贝需要的非系统 DLL 到输出目录
# 用法: cmake -DTARGET_FILE=<path> -DOUT_DIR=<dir> -DSEARCH_DIRS=<dirs> -P copy_dlls.cmake

include(GetPrerequisites)

set(GP_APPEND_UNKNOWN TRUE)

# --- Step 0: 构建搜索路径 ---
file(GLOB bundle_bin_dirs "${SEARCH_DIRS}/*/bin")
# Bundle dirs first: otherwise a dependency already present in OUT_DIR resolves
# back to its own (possibly stale) copy. OUT_DIR last still resolves in-tree DLLs.
set(_search_paths ${bundle_bin_dirs} "${OUT_DIR}")

# 过滤掉不存在的路径
set(search_paths "")
foreach(p ${_search_paths})
    if(EXISTS "${p}")
        list(APPEND search_paths "${p}")
    endif()
endforeach()

message(STATUS "[copy_dlls] Target: ${TARGET_FILE}")
message(STATUS "[copy_dlls] OutDir: ${OUT_DIR}")

# --- Step 1: 递归获取所有非系统依赖 ---
get_prerequisites("${TARGET_FILE}" DEPS 1 1 "" "${search_paths}")

set(copied "")
set(missing "")

foreach(DEP ${DEPS})
    # 跳过 OwnDLL（通常是 exe/dll 自己引用的导出符号）
    string(REGEX MATCH "OwnDLL" _is_own "${DEP}")
    if(_is_own)
        continue()
    endif()

    gp_resolve_item("${TARGET_FILE}" "${DEP}" "" "${search_paths}" resolved)
    if(NOT resolved OR resolved STREQUAL "")
        message(WARNING "[copy_dlls] Not found: ${DEP}")
        list(APPEND missing "${DEP}")
        continue()
    endif()

    gp_resolved_file_type("${TARGET_FILE}" "${resolved}" "" "${search_paths}" ftype)
    if(ftype STREQUAL "system")
        continue()
    endif()

    get_filename_component(fname "${resolved}" NAME)

    # Skip only when the copy already in OUT_DIR is identical to the bundle source;
    # otherwise fall through and overwrite the stale one.
    if(EXISTS "${OUT_DIR}/${fname}")
        file(MD5 "${resolved}" _src_md5)
        file(MD5 "${OUT_DIR}/${fname}" _dst_md5)
        if(NOT _src_md5 STREQUAL "" AND _src_md5 STREQUAL _dst_md5)
            continue()
        endif()
    endif()

    # 去重
    list(FIND copied "${fname}" _idx)
    if(_idx GREATER -1)
        continue()
    endif()

    message(STATUS "[copy_dlls] Copy: ${fname}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${resolved}" "${OUT_DIR}")
    list(APPEND copied "${fname}")
endforeach()

string(LENGTH "${copied}" _len)
if(_len GREATER 0)
    list(LENGTH copied copied_len)
    message(STATUS "[copy_dlls] Copied ${copied_len} DLLs")
else()
    message(STATUS "[copy_dlls] No new DLLs to copy")
endif()

if(missing)
    list(LENGTH missing missing_len)
    message(WARNING "[copy_dlls] ${missing_len} DLL(s) not found: ${missing}")
endif()
