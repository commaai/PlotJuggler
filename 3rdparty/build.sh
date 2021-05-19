# builds cereal and opendbc
# will NOT run outside docker container
cp opendbc/SConstruct . && cp -r opendbc/site_scons . && scons -j4
