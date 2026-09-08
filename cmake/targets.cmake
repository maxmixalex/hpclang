# Central hpclang model registry.

set(HPCLANG_SUPPORTED_TARGETS
    hp39gplus
    hp39gs
    hp40gs
    hp48gii
    hp49gplus
    hp50g
)

function(hpclang_target_family target out_var)
    if(target STREQUAL "hp39gplus" OR
       target STREQUAL "hp39gs" OR
       target STREQUAL "hp40gs")
        set(_family "hp39")
    elseif(target STREQUAL "hp48gii")
        set(_family "hp48")
    elseif(target STREQUAL "hp49gplus" OR
           target STREQUAL "hp50g")
        set(_family "hp4950")
    else()
        message(FATAL_ERROR
            "Unknown HPCLANG_TARGET='${target}'. "
            "Supported targets: ${HPCLANG_SUPPORTED_TARGETS}")
    endif()

    set(${out_var} "${_family}" PARENT_SCOPE)
endfunction()

function(hpclang_validate_target target)
    list(FIND HPCLANG_SUPPORTED_TARGETS "${target}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR
            "Unsupported HPCLANG_TARGET='${target}'. "
            "Supported targets: ${HPCLANG_SUPPORTED_TARGETS}")
    endif()
endfunction()
