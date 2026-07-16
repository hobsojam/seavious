# Deploy every game asset by content rather than timestamp.  This matters
# when building from Windows against a WSL checkout: an earlier interrupted
# copy can leave zero-byte destination files with newer timestamps.

file(GLOB_RECURSE asset_files
    LIST_DIRECTORIES false
    RELATIVE "${ASSET_SOURCE_DIR}"
    "${ASSET_SOURCE_DIR}/*"
)

foreach(asset_file IN LISTS asset_files)
    get_filename_component(asset_dir "${ASSET_DEST_DIR}/${asset_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${asset_dir}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ASSET_SOURCE_DIR}/${asset_file}"
            "${ASSET_DEST_DIR}/${asset_file}"
        RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Could not deploy asset: ${asset_file}")
    endif()
endforeach()
