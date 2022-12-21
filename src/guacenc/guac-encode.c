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

#include "config.h"
#include "display.h"
#include "instructions.h"
#include "log.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/parser.h>
#include <guacamole/socket.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/**
 * Reads and handles all Guacamole instructions from the given guac_socket
 * until end-of-stream is reached.
 *
 * @param display
 *     The current internal display of the Guacamole video encoder.
 *
 * @param path
 *     The name of the file being parsed (for logging purposes). This file
 *     must already be open and available through the given socket.
 *
 * @param socket
 *     The guac_socket through which instructions should be read.
 *
 * @return
 *     Zero on success, non-zero if parsing of Guacamole protocol data through
 *     the given socket fails.
 */
static int guacenc_read_instructions(guacenc_display* display,
        const char* path, guac_socket* socket) {

    /* Obtain Guacamole protocol parser */
    guac_parser* parser = guac_parser_alloc();
    if (parser == NULL)
        return 1;

    /* Continuously read and handle all instructions */
    while (!guac_parser_read(parser, socket, -1)) {
        if (guacenc_handle_instruction(display, parser->opcode,
                parser->argc, parser->argv)) {
            guacenc_log(GUAC_LOG_DEBUG, "Handling of \"%s\" instruction "
                    "failed.", parser->opcode);
        }
    }

    /* Fail on read/parse error */
    if (guac_error != GUAC_STATUS_CLOSED) {
        guacenc_log(GUAC_LOG_ERROR, "%s: %s",
                path, guac_status_string(guac_error));
        guac_parser_free(parser);
        return 1;
    }

    /* Parse complete */
    guac_parser_free(parser);
    return 0;

}

static int get_config_v1(const char* path, int* width, int* height) {
    /* Open input file */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        guacenc_log(GUAC_LOG_ERROR, "%s: %s", path, strerror(errno));
        return 1;
    }

    /* Obtain guac_socket wrapping file descriptor */
    guac_socket* socket = guac_socket_open(fd);
    if (socket == NULL) {
        guacenc_log(GUAC_LOG_ERROR, "%s: %s", path,
                guac_status_string(guac_error));
        close(fd);
        return 1;
    }

    /* Attempt to read all instructions in the file */

    /* Obtain Guacamole protocol parser */
    guac_parser* parser = guac_parser_alloc();
    if (parser == NULL) {
        guac_socket_free(socket);
        close(fd);
        return 1;
    }

    /* Continuously read and handle all instructions */
    while (!guac_parser_read(parser, socket, -1)) {

        if (strcmp(parser->opcode, "size") == 0) {
            // parser->argc, parser->argv
            char** argv = parser->argv;
            /* Verify argument count */
            if (parser->argc < 3) {
                guacenc_log(GUAC_LOG_WARNING, "\"size\" instruction incomplete");
                return 1;
            }

            /* Parse arguments */
            int index = atoi(argv[0]);
            if (index ==0 ){
                *width = atoi(argv[1]);
                *height = atoi(argv[2]);

                guacenc_log(GUAC_LOG_INFO, "Handling of \"%s\" instruction; index=%d; width=%d; height=%d ;", parser->opcode, index, *width, *height);

                break;
            }
        }
    }

    /* Parse complete */
    guac_parser_free(parser);
    /* Close input and finish encoding process */
    guac_socket_free(socket);
    close(fd);
    return 0;
}

extern int get_parser_code(const char* opcode, int* argc, char** argv, bool* status);

int guac_encode_v1(int(*get_parser_code)(char*, int*, char**, bool*) ,const char* out_path, const char* codec,
        int width, int height, int bitrate, bool force) {
    guacenc_log(GUAC_LOG_INFO, "Video will be encoded at %ix%i "
            "and %i bps.", width, height, bitrate);

    /* Allocate display for encoding process */
    guacenc_display* display = guacenc_display_alloc(out_path, codec,
            width, height, bitrate);
    if (display == NULL) {
        return 1;
    }

    guacenc_log(GUAC_LOG_INFO, "Encoding to \"%s\" ...", out_path);

    guac_parser* parser = guac_parser_alloc();
    if (parser == NULL) {
        guacenc_display_free(display);
        return 1;
    }
              

    char* opcode = parser->opcode;
    char** argv = parser->argv;
    int argc = 0;
    bool status = true;
    while (get_parser_code(opcode, &argc, argv, &status) == 0)
    {
        if (guacenc_handle_instruction(display, opcode, argc, argv)) {
            guacenc_log(GUAC_LOG_DEBUG, "Handling of \"%s\" instruction "
                    "failed.", *opcode);
            guac_parser_free(parser);
            guacenc_display_free(display);
            status = false;
            return 1;
        }
    }
    guac_parser_free(parser);

    /* Close input and finish encoding process */
    return guacenc_display_free(display);

}

int guac_encode_from_file(const char* path, const char* out_path, const char* codec,
        int width, int height, int bitrate, bool force) {
    if (width <= 0 || height <= 0) {
        get_config_v1(path, &width, &height);
    }
    guacenc_log(GUAC_LOG_INFO, "Video will be encoded at %ix%i "
            "and %i bps.", width, height, bitrate);
    /* Open input file */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        guacenc_log(GUAC_LOG_ERROR, "%s: %s", path, strerror(errno));
        return 1;
    }

    /* Lock entire input file for reading by the current process */
    struct flock file_lock = {
        .l_type   = F_RDLCK,
        .l_whence = SEEK_SET,
        .l_start  = 0,
        .l_len    = 0,
        .l_pid    = getpid()
    };

    /* Abort if file cannot be locked for reading */
    if (!force && fcntl(fd, F_SETLK, &file_lock) == -1) {

        /* Warn if lock cannot be acquired */
        if (errno == EACCES || errno == EAGAIN)
            guacenc_log(GUAC_LOG_WARNING, "Refusing to encode in-progress "
                    "recording \"%s\" (specify the -f option to override "
                    "this behavior).", path);

        /* Log an error if locking fails in an unexpected way */
        else
            guacenc_log(GUAC_LOG_ERROR, "Cannot lock \"%s\" for reading: %s",
                    path, strerror(errno));

        close(fd);
        return 1;
    }

    /* Allocate display for encoding process */
    guacenc_display* display = guacenc_display_alloc(out_path, codec,
            width, height, bitrate);
    if (display == NULL) {
        close(fd);
        return 1;
    }

    /* Obtain guac_socket wrapping file descriptor */
    guac_socket* socket = guac_socket_open(fd);
    if (socket == NULL) {
        guacenc_log(GUAC_LOG_ERROR, "%s: %s", path,
                guac_status_string(guac_error));
        close(fd);
        guacenc_display_free(display);
        return 1;
    }

    guacenc_log(GUAC_LOG_INFO, "Encoding \"%s\" to \"%s\" ...", path, out_path);

    /* Attempt to read all instructions in the file */
    if (guacenc_read_instructions(display, path, socket)) {
        guac_socket_free(socket);
        guacenc_display_free(display);
        return 1;
    }

    /* Close input and finish encoding process */
    guac_socket_free(socket);
    return guacenc_display_free(display);

}
