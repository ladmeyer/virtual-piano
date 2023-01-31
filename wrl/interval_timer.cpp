void app_interval_timer() {
    char *filename = "codebase/316818__steaq__432-hz.wav";
    char buffer[128] = {};
    int num_chars = 0;
    real interval_time_in_minutes = 15.0;
    real timestamp = util_timestamp_in_milliseconds();
    int num_badges = 0;
    sound_play_sound(filename);
    COW1._gui_hide_and_disable = true;
    config.tweaks_soup_draw_with_rounded_corners_for_all_line_primitives = false;
    config.hotkeys_app_quit = '\0';

    while (cow_begin_frame()) {
        gui_printf("> %s", buffer);
        gui_readout("interval_time_in_minutes", &interval_time_in_minutes);

        static char keys[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.' };
        for_(i, _COUNT_OF(keys)) {
            if (globals.key_pressed[keys[i]]) {
                buffer[num_chars++] = keys[i];
            }
        }
        if (globals.key_pressed[COW_KEY_BACKSPACE] && num_chars > 0) {
            buffer[--num_chars] = '\0';
        }
        if (globals.key_pressed[COW_KEY_ENTER]) {
            sound_play_sound(filename);
            if (num_chars > 0) {
                sscanf(buffer, "%lf", &interval_time_in_minutes);
                num_chars = 0;
                memset(buffer, 0, _COUNT_OF(buffer));
            }
            num_badges = 0;
            timestamp = util_timestamp_in_milliseconds();
        }
        real minutes_per_interval = (util_timestamp_in_milliseconds() - timestamp) / (1000 * 60);
        real f = minutes_per_interval / interval_time_in_minutes;
        gui_readout("f", &f);
        if (f > 1.0) {
            sound_play_sound(filename);
            timestamp = util_timestamp_in_milliseconds();
            ++num_badges;
        }

        {
            eso_begin(globals.Identity, SOUP_LINES, 64);
            eso_color(color_plasma(f));
            eso_vertex(-1.0, 0.0);
            eso_vertex(LERP(f, -1.0, 1.0), 0.0);
            eso_end();
        }
        {
            eso_begin(globals.Identity, SOUP_POINTS, 32);
            eso_color(color_plasma(f));
            for (int i = 0; i < num_badges; ++i) {
                eso_vertex(-1.0 + (i + 1) * .05, 1.0 - 0.05);
            }
            eso_end();
        }
    }
}