
update-deps: update-sokol update-imgui

update-sokol:
	wget -O deps/sokol_app.h https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h
	wget -O deps/sokol_gfx.h https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h
	wget -O deps/sokol_glue.h https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h
	wget -O deps/sokol_log.h https://raw.githubusercontent.com/floooh/sokol/master/sokol_log.h
	wget -O deps/sokol_imgui.h https://github.com/floooh/sokol/raw/refs/heads/master/util/sokol_imgui.h

update-cimgui: update-imgui
	wget -O deps/cimgui/cimconfig.h https://raw.githubusercontent.com/cimgui/cimgui/refs/heads/docking_inter/cimconfig.h
	wget -O deps/cimgui/cimgui.cpp https://raw.githubusercontent.com/cimgui/cimgui/refs/heads/docking_inter/cimgui.cpp
	wget -O deps/cimgui/cimgui.h https://raw.githubusercontent.com/cimgui/cimgui/refs/heads/docking_inter/cimgui.h

update-imgui:
	wget -O deps/cimgui/imgui/imconfig.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imconfig.h
	wget -O deps/cimgui/imgui/imgui.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui.h
	wget -O deps/cimgui/imgui/imgui.cpp https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui.cpp
	wget -O deps/cimgui/imgui/imgui_demo.cpp https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui_demo.cpp
	wget -O deps/cimgui/imgui/imgui_draw.cpp https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui_draw.cpp
	wget -O deps/cimgui/imgui/imgui_internal.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui_internal.h
	wget -O deps/cimgui/imgui/imgui_tables.cpp https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui_tables.cpp
	wget -O deps/cimgui/imgui/imgui_widgets.cpp https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imgui_widgets.cpp
	wget -O deps/cimgui/imgui/imstb_rectpack.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imstb_rectpack.h
	wget -O deps/cimgui/imgui/imstb_textedit.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imstb_textedit.h
	wget -O deps/cimgui/imgui/imstb_truetype.h https://raw.githubusercontent.com/ocornut/imgui/62275e877a691bd2e51d1ca86bdfed49fa5cb297/imstb_truetype.h

update-sqlite3:
	wget -O deps/sqlite-amalgamation.zip https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
	unzip -j deps/sqlite-amalgamation.zip -d deps/
	rm deps/sqlite-amalgamation.zip

update-arena:
	wget -O deps/arena.h https://raw.githubusercontent.com/tsoding/arena/refs/heads/master/arena.h
