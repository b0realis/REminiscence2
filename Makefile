
SDL_CFLAGS   := `sdl2-config --cflags`
SDL_LIBS     := `sdl2-config --libs`

MODPLUG_LIBS := -lmodplug
TREMOR_LIBS  := #-lvorbisidec -logg
ZLIB_LIBS    := -lz

LIBS = $(SDL_LIBS) $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

CXX=g++
CXXFLAGS += -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wpedantic -Woverlength-strings -MMD $(SDL_CFLAGS) -DUSE_MODPLUG -DUSE_STB_VORBIS -DUSE_ZLIB
CXXFLAGS += -Ithird_party/imgui -Ithird_party/imgui/backends

SRCS = cli_text.cpp collision.cpp config.cpp cpc_player.cpp cutscene.cpp cutscene_assets.cpp cutscene_tool.cpp decode_mac.cpp decode_sega.cpp file.cpp fs.cpp game.cpp graphics.cpp main.cpp \
	menu.cpp mixer.cpp music_midi.cpp music_prf.cpp music_synth.cpp mod_player.cpp ogg_player.cpp third_party/nuked-opl3/opl3.c \
	piege.cpp protection.cpp resource.cpp resource_aba.cpp \
	resource_mac.cpp resource_paq.cpp scaler.cpp screenshot.cpp seq_player.cpp \
	sfx_player.cpp staticres.cpp staticres_controllers.cpp systemstub_sdl.cpp unpack.cpp util.cpp video.cpp
SRCS += game_settings.cpp settings.cpp settings_store.cpp settings_overlay.cpp keyboard_diagram.cpp save_catalog.cpp appearance.cpp \
	third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp \
	third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_sdl2.cpp \
	third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp

#CXXFLAGS += -DUSE_STATIC_SCALER
#SCALERS  := scalers/scaler_nearest.cpp scalers/scaler_tv2x.cpp scalers/scaler_xbr.cpp

#CXXFLAGS    += -DUSE_MIDI_DRIVER
#MIDIDRIVERS := midi_driver_adlib.cpp midi_driver_mt32.cpp
#MIDI_LIBS   := -lmt32emu

LIBS = $(MIDI_LIBS) $(MODPLUG_LIBS) $(SDL_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

OBJS = $(patsubst %.c,%.o,$(SRCS:.cpp=.o)) $(SCALERS:.cpp=.o) $(MIDIDRIVERS:.cpp=.o)
DEPS = $(patsubst %.c,%.d,$(SRCS:.cpp=.d)) $(SCALERS:.cpp=.d) $(MIDIDRIVERS:.cpp=.d)

fb: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
