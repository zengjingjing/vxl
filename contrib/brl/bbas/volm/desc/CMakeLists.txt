# volm/desc/CMakeLists.txt
# Existence matching and base matching library

INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bbas/volm )
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bbas/bsta )
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bbas/bpgl )

SET(volm_desc_sources
  volm_desc.h                             volm_desc.cxx
  volm_desc_ex.h                          volm_desc_ex.cxx
  volm_desc_land.h                        volm_desc_land.cxx
  volm_desc_matcher.h                     volm_desc_matcher.cxx
  volm_desc_indexer.h					  volm_desc_indexer.cxx
  volm_desc_land_matcher.h				  volm_desc_land_matcher.cxx
  volm_desc_land_indexer.h                volm_desc_land_indexer.cxx
  volm_desc_ex_matcher.h                  volm_desc_ex_matcher.cxx
)

AUX_SOURCE_DIRECTORY(Templates volm_desc_sources)

ADD_LIBRARY(volm_desc ${volm_desc_sources})

TARGET_LINK_LIBRARIES(volm_desc volm bsta bsta_algo bsta_vis bsvg bvgl vsl vbl vbl_io vul depth_map boxm2_volm_desc)

IF(BUILD_TESTING)
  SUBDIRS(tests)
ENDIF(BUILD_TESTING)
 
SUBDIRS(exe)