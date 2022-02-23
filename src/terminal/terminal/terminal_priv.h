/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#ifndef _GUAC_TERMINAL_PRIV_H
#define _GUAC_TERMINAL_PRIV_H

#include "common/clipboard.h"
#include "terminal/terminal.h"

struct guac_terminal {

    /**
     * The Guacamole client associated with this terminal emulator.
     */
    guac_client* client;

    /**
     * Whether user input should be handled and this terminal should render
     * frames. Initially, this will be false, user input will be ignored, and
     * rendering of frames will be withheld until guac_terminal_start() has
     * been invoked. The data within frames will still be rendered, and text
     * data received will still be handled, however actual frame boundaries
     * will not be sent.
     */
    bool started;

    /**
     * The terminal render thread.
     */
    pthread_t thread;

    /**
     * Called whenever the necessary terminal codes are sent to change
     * the path for future file uploads.
     */
    guac_terminal_upload_path_handler* upload_path_handler;

    /**
     * Called whenever the necessary terminal codes are sent to initiate
     * a download of a given remote file.
     */
    guac_terminal_file_download_handler* file_download_handler;

    /**
     * Lock which restricts simultaneous access to this terminal via the root
     * guac_terminal_* functions.
     */
    pthread_mutex_t lock;

    /**
     * The mutex associated with the modified condition and flag, locked
     * whenever a thread is waiting on the modified condition, the modified
     * condition is being signalled, or the modified flag is being changed.
     */
    pthread_mutex_t modified_lock;

    /**
     * Flag set whenever an operation has affected the terminal in a way that
     * will require a frame flush. When this flag is set, the modified_cond
     * condition will be signalled. The modified_lock will always be
     * acquired before this flag is altered.
     */
    int modified;

    /**
     * Condition which is signalled when the modified flag has been set
     */
    pthread_cond_t modified_cond;

    /**
     * Pipe which will be the source of user input. When a terminal code
     * generates synthesized user input, that data will be written to
     * this pipe.
     */
    int stdin_pipe_fd[2];

    /**
     * The currently-open pipe stream from which all terminal input should be
     * read, if any. If no pipe stream is open, terminal input will be received
     * through keyboard, clipboard, and mouse events, and this value will be
     * NULL.
     */
    guac_stream* input_stream;

    /**
     * The currently-open pipe stream to which all terminal output should be
     * written, if any. If no pipe stream is open, terminal output will be
     * written to the terminal display, and this value will be NULL.
     */
    guac_stream* pipe_stream;

    /**
     * Bitwise OR of all flags which apply to the currently-open pipe stream.
     * If no pipe stream is open, this value has no meaning, and its contents
     * are undefined.
     *
     * @see GUAC_TERMINAL_PIPE_INTERPRET_OUTPUT
     * @see GUAC_TERMINAL_PIPE_AUTOFLUSH
     */
    int pipe_stream_flags;

    /**
     * Buffer of data pending write to the pipe_stream. Data within this buffer
     * will be flushed to the pipe_stream when either (1) the buffer is full
     * and another character needs to be written or (2) the pipe_stream is
     * closed.
     */
    char pipe_buffer[6048];

    /**
     * The number of bytes currently stored within the pipe_buffer.
     */
    int pipe_buffer_length;

    /**
     * The currently-active typescript recording all terminal output, or NULL
     * if no typescript is being used for the terminal session.
     */
    guac_terminal_typescript* typescript;

    /**
     * Terminal-wide mouse cursor, synchronized across all users.
     */
    guac_common_cursor* cursor;

    /**
     * Graphical representation of the current scroll state.
     */
    guac_terminal_scrollbar* scrollbar;

    /**
     * The relative offset of the display. A positive value indicates that
     * many rows have been scrolled into view, zero indicates that no
     * scrolling has occurred. Negative values are illegal.
     */
    int scroll_offset;

    /**
     * The maximum number of rows to allow within the terminal buffer. Note
     * that while this value is traditionally referred to as the scrollback
     * size, it actually encompasses both the display and the off-screen
     * region. The terminal will ensure enough buffer space is allocated for
     * the on-screen rows, even if this exceeds the defined maximum, however
     * additional rows for off-screen data will only be available if the
     * display is smaller than this value.
     */
    int max_scrollback;

    /**
     * The number of rows that the user has requested be avalable within the
     * terminal buffer. This value may be adjusted by the user while the
     * terminal is running through console codes, and will adjust the number
     * of rows available within the terminal buffer, subject to the maximum
     * defined at terminal creation and stored within max_scrollback.
     */
    int requested_scrollback;

    /**
     * The width of the space available to all components of the terminal, in
     * pixels. This may include space which will not actually be used for
     * character rendering.
     */
    int outer_width;

    /**
     * The height of the space available to all components of the terminal, in
     * pixels. This may include space which will not actually be used for
     * character rendering.
     */
    int outer_height;

    /**
     * The width of the terminal, in pixels.
     */
    int width;

    /**
     * The height of the terminal, in pixels.
     */
    int height;

    /**
     * The width of the terminal, in characters.
     */
    int term_width;

    /**
     * The height of the terminal, in characters.
     */
    int term_height;

    /**
     * The index of the first row in the scrolling region.
     */
    int scroll_start;

    /**
     * The index of the last row in the scrolling region.
     */
    int scroll_end;

    /**
     * The current row location of the cursor. Note that while most terminal
     * operations will clip the cursor location within the bounds of the
     * terminal, this is not guaranteed.
     */
    int cursor_row;

    /**
     * The current column location of the cursor. Note that while most
     * terminal operations will clip the cursor location within the bounds
     * of the terminal, this is not guaranteed. There are times when the
     * cursor is legitimately outside the terminal bounds (such as when the
     * end of a line is reached, but it is not yet necessary to scroll up).
     */
    int cursor_col;

    /**
     * The desired visibility state of the cursor.
     */
    bool cursor_visible;

    /**
     * The row of the rendered cursor.
     * Will be set to -1 if the cursor is not visible.
     */
    int visible_cursor_row;

    /**
     * The column of the rendered cursor.
     * Will be set to -1 if the cursor is not visible.
     */
    int visible_cursor_col;

    /**
     * The row of the saved cursor (ESC 7).
     */
    int saved_cursor_row;

    /**
     * The column of the saved cursor (ESC 7).
     */
    int saved_cursor_col;

    /**
     * The attributes which will be applied to future characters.
     */
    guac_terminal_attributes current_attributes;

    /**
     * The character whose attributes dictate the default attributes
     * of all characters. When new screen space is allocated, this
     * character fills the gaps.
     */
    guac_terminal_char default_char;

    /**
     * Handler which will receive all printed characters, updating the terminal
     * accordingly.
     */
    guac_terminal_char_handler* char_handler;

    /**
     * The difference between the currently-rendered screen and the current
     * state of the terminal, and the contextual information necessary to
     * interpret and render those differences.
     */
    guac_terminal_display* display;

    /**
     * Current terminal display state. All characters present on the screen
     * are within this buffer. This has nothing to do with the display, which
     * facilitates transfer of a set of changes to the remote display.
     */
    guac_terminal_buffer* buffer;

    /**
     * Automatically place a tabstop every N characters. If zero, then no
     * tabstops exist automatically.
     */
    int tab_interval;

    /**
     * Array of all tabs set. Each entry is the column number of a tab + 1,
     * or 0 if that tab cell is unset.
     */
    int custom_tabs[GUAC_TERMINAL_MAX_TABS];

    /**
     * Array of arrays of mapped characters, where the character N is located at the N-32
     * position within the array. Each element in a contained array is the corresponding Unicode
     * codepoint. If NULL, a direct mapping from Unicode is used. The entries of the main array
     * correspond to the character set in use (G0, G1, etc.)
     */
    const int* char_mapping[2];

    /**
     * The active character set. For example, 0 for G0, 1 for G1, etc.
     */
    int active_char_set;

    /**
     * Whether text is currently selected.
     */
    bool text_selected;

    /**
     * Whether the selection is finished, and will no longer be modified. A
     * committed selection remains highlighted for reference, but the
     * highlight will be removed if characters within the selected region are
     * modified.
     */
    bool selection_committed;

    /**
     * The row that the selection starts at.
     */
    int selection_start_row;

    /**
     * The column that the selection starts at.
     */
    int selection_start_column;

    /**
     * The width of the character at selection start.
     */
    int selection_start_width;

    /**
     * The row that the selection ends at.
     */
    int selection_end_row;

    /**
     * The column that the selection ends at.
     */
    int selection_end_column;

    /**
     * The width of the character at selection end.
     */
    int selection_end_width;

    /**
     * Whether the cursor (arrow) keys should send cursor sequences
     * or application sequences (DECCKM).
     */
    bool application_cursor_keys;

    /**
     * Whether a CR should automatically follow a LF, VT, or FF.
     */
    bool automatic_carriage_return;

    /**
     * Whether insert mode is enabled (DECIM).
     */
    bool insert_mode;

    /**
     * Whether the alt key is currently being held down.
     */
    int mod_alt;

    /**
     * Whether the control key is currently being held down.
     */
    int mod_ctrl;

    /**
     * Whether the shift key is currently being held down.
     */
    int mod_shift;

    /**
     * The current mouse button state.
     */
    int mouse_mask;

    /**
     * The current mouse cursor, to avoid re-setting the cursor image.
     */
    guac_terminal_cursor_type current_cursor;

    /**
     * The current contents of the clipboard. This clipboard instance is
     * maintained externally (will not be freed when this terminal is freed)
     * and will be updated both internally by the terminal and externally
     * through received clipboard instructions.
     */
    guac_common_clipboard* clipboard;

    /**
     * The name of the font to use when rendering glyphs, as requested at
     * creation time or via guac_terminal_apply_font().
     */
    const char* font_name;

    /**
     * The size of each glyph, in points, as requested at creation time or via
     * guac_terminal_apply_font().
     */
    int font_size;

    /**
     * The name of the color scheme to use, as requested at creation time or
     * via guac_terminal_apply_color_scheme(). This string must be in the
     * format accepted by guac_terminal_parse_color_scheme().
     */
    const char* color_scheme;

    /**
     * ASCII character to send when backspace is pressed.
     */
    char backspace;

    /**
     * Whether copying from the terminal clipboard should be blocked. If set,
     * the contents of the terminal can still be copied, but will be usable
     * only within the terminal itself. The clipboard contents will not be
     * automatically streamed to the client.
     */
    bool disable_copy;

};

#endif

