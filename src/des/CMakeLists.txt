target_include_directories( bmxcore
   PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)

target_sources( bmxcore
   PRIVATE
   bmx_des_K.H
   bmx_pc.cpp
   bmx_pc.H
   bmx_pc_init.cpp
   bmx_calc_volume_fraction.cpp
   bmx_calc_txfr.cpp
   )

