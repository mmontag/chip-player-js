AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libadlmidi_plugin.la

CFLAGS = -DVLC_MODULE_COPYRIGHT="\"Copyright \(c\) Vitaly Novichkov\""
CFLAGS += -DVLC_MODULE_LICENSE=\"GPLv3\"

libadlmidi_plugin_la_SOURCES = libadlmidi.c
libadlmidi_plugin_la_CFLAGS = -DMODULE_STRING=\"adlmidi\" $(vlc_CFLAGS)
libadlmidi_plugin_la_CPPFLAGS = -DMODULE_STRING=\"adlmidi\" $(vlc_CFLAGS)
libadlmidi_plugin_la_LIBADD = $(vlc_LIBS)
libadlmidi_plugin_la_LDFLAGS = -avoid-version -module -export-symbol-regex ^vlc_entry -lADLMIDI -lm -lstdc++ $(vlc_LDFLAGS)

