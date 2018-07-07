# rtl_433 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# rtl_433 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Build utility
########################################################################
add_executable(rtl_433
	baseband.c
	bitbuffer.c
	data.c
	pulse_demod.c
	pulse_detect.c
	rtl_433.c
	optparse.c
	util.c
	devices/flex.c
	devices/fineoffset_wh1080.c
)

add_library(data data.c)

target_link_libraries(rtl_433
	${SDR_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)


set(INSTALL_TARGETS rtl_433)
if(UNIX)
target_link_libraries(rtl_433 m)
endif()

# Explicitly say that we want C99
set_property(TARGET rtl_433 PROPERTY C_STANDARD 99)

########################################################################
# Install built library files & utilities
########################################################################
install(TARGETS ${INSTALL_TARGETS}
    RUNTIME DESTINATION bin              # .dll file
)
