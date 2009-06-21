cmake_minimum_required(VERSION 2.6)
project(xmp)
include_directories(src/include .)
add_definitions(-DSYSCONFDIR=\"/etc/xmp\")

set(XMP src/main/xmp)

set(PLAYER_SOURCES
	convert.c cruncher.c driver.c filter.c fmopl.c formats.c
	med_synth.c misc.c mix_all.c period.c player.c readrc.c
	scan.c synth.c ulaw.c
)

set(LOADERS_SOURCES
        xm_load.c mod_load.c s3m_load.c stm_load.c 669_load.c far_load.c
        mtm_load.c ptm_load.c okt_load.c alm_load.c amd_load.c rad_load.c
        ult_load.c mdl_load.c it_load.c stx_load.c pt3_load.c sfx_load.c
        flt_load.c st_load.c emod_load.c imf_load.c digi_load.c fnk_load.c
        ice_load.c hsc_load.c liq_load.c ims_load.c masi_load.c amf_load.c
        psm_load.c stim_load.c mmd1_load.c mmd3_load.c rtm_load.c dmf_load.c
	tcb_load.c dt_load.c gtk_load.c no_load.c arch_load.c sym_load.c
	dtt_load.c mgt_load.c med3_load.c med4_load.c ssmt_load.c dbm_load.c
	umx_load.c gdm_load.c coco_load.c pw_load.c
        common.c iff.c itsex.c asif.c
)

set(PROWIZARD_SOURCES
	prowiz.c ptktable.c tuning.c
	ac1d.c di.c eureka.c fc-m.c fuchs.c fuzzac.c gmc.c heatseek.c
	kris.c ksm.c mp.c np1.c np2.c np3.c p60a.c pm10c.c pm18a.c pha.c
	prun1.c prun2.c tdd.c unic.c unic2.c wn.c zen.c tp3.c p40.c xann.c
)

set(MISC_SOURCES
	control.c info.c readdata.c load.c ppdepack.c unsqsh.c
	mmcmp.c readrle.c readhuff.c readlzw.c unarc.c arcfs.c
	oxm.c mkstemp.c xfd.c
)

set(MAIN_SOURCES
	getopt.c getopt1.c options.c main.c
)


macro(ADD_DIR name dir)
	foreach(i ${${name}})
		list(APPEND SOURCE_LIST ${dir}/${i})
	endforeach(i ${${name}})
endmacro(ADD_DIR name dir)

ADD_DIR(PLAYER_SOURCES src/player)
ADD_DIR(LOADERS_SOURCES src/loaders)
ADD_DIR(PROWIZARD_SOURCES src/loaders/prowizard)
ADD_DIR(MISC_SOURCES src/misc)
ADD_DIR(MAIN_SOURCES src/main)

#
# Configuration
#

add_definitions(-DHAVE_CONFIG_H)

# File and wav output are always available
list(APPEND SOURCE_LIST src/drivers/file.c src/drivers/wav.c)

# Check OSS files
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
CHECK_INCLUDE_FILES(sys/ultrasound.h HAVE_SYS_ULTRASOUND_H)

if (HAVE_SYS_SOUNDCARD_H)
	list(APPEND SOURCE_LIST src/drivers/oss.c)
	list(APPEND SOURCE_LIST src/drivers/oss_seq.c)
endif (HAVE_SYS_SOUNDCARD_H)

add_executable(${XMP} ${SOURCE_LIST})
target_link_libraries(${XMP} -lm)

CONFIGURE_FILE(src/include/config.h.cmake src/include/config.h)
