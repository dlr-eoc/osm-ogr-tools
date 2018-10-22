add_executable(osm2ogr_tagged osm2ogr_tagged.cpp)
target_link_libraries(osm2ogr_tagged ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})
set_pthread_on_target(osm2ogr_tagged)
install(TARGETS osm2ogr_tagged DESTINATION bin)

