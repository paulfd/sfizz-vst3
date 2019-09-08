add_subdirectory(sfizz)

set(sfizz_vst_sources
	 ${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
    ${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.h
    sfizz-vst.cpp
    version.h)

set(targetsfizz sfizz-vst)
smtg_add_vst3plugin(${targetsfizz} ${sfizz_vst_sources})
target_include_directories(${targetsfizz} PUBLIC ${VSTGUI_ROOT}/vstgui4)
target_link_libraries(${targetsfizz} PRIVATE base sdk vstgui_support sfizz::sfizz absl::str_format)

# set(again_simple_sources
#     ${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
#     ${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.h
#     againparamids.h
#     againsimple.cpp 
#     againsimple.h
#     version.h
# )

# set(targetsimple againsimple)
# smtg_add_vst3plugin(${targetsimple} ${again_simple_sources})
# target_include_directories(${targetsimple} PUBLIC ${VSTGUI_ROOT}/vstgui4)
# target_link_libraries(${targetsimple} PRIVATE base sdk vstgui_support sfizz::sfizz)

# smtg_add_vst3_resource(${targetsimple} "resource/again.uidesc")
# smtg_add_vst3_resource(${targetsimple} "resource/background.png")
# smtg_add_vst3_resource(${targetsimple} "resource/slider_background.png")
# smtg_add_vst3_resource(${targetsimple} "resource/slider_handle.png")
# smtg_add_vst3_resource(${targetsimple} "resource/slider_handle_2.0x.png")
# smtg_add_vst3_resource(${targetsimple} "resource/vu_on.png")
# smtg_add_vst3_resource(${targetsimple} "resource/vu_off.png")
