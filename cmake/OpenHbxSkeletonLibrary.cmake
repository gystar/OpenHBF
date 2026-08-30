function(openhbx_add_skeleton_library target source public_header)
  add_library(${target} STATIC ${source} ${public_header})
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${target} PRIVATE openhbf_warnings)
  set_property(TARGET ${target} PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
endfunction()
