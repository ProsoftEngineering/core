if(NOT PSCoreCMakeUtils_FOUND)

# Append "-fobjc-arc" to COMPILE_OPTIONS on target .m/.mm SOURCES
function(ps_core_use_objc_arc TARGET_NAME)
    get_target_property(SOURCE_FILES ${TARGET_NAME} SOURCES)
    foreach(SOURCE_FILE IN LISTS SOURCE_FILES)
        if(SOURCE_FILE MATCHES "\\.mm?\$")      # .m and .mm files
            set_property(SOURCE ${SOURCE_FILE}
                APPEND PROPERTY COMPILE_OPTIONS "-fobjc-arc"
            )
        endif()
    endforeach()
endfunction()

endif() # PSCoreCMakeUtils_FOUND
