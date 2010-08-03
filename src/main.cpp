/*  
 *  main.cpp
 *
 *  Copyright (c) 2010 Motiejus Jakštys
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.

 *  This program is distributed in the hope that it will be usefu
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>
#include "main.h"
#include "soundpatty.h"
#include "fileinput.h"
#ifdef HAVE_JACK
#include "jackinput.h"
#endif // HAVE_JACK

action_t action;
enum channel_hook_way_t { AUTO, MANUAL } channel_hook_way = MANUAL;

void its_over(const char* noop, double place) {
    printf("FOUND, processed %.6f sec\n", place);
    exit(0);
};
void usage() {
    perror (
        "soundpatty <options> [channel/file]name\n\n"

        "Options:\n"
        "  -a  action (always mandatory, see below)\n"
        "  -c  config file (always mandatory)\n"
        "  -s  sample file (only if action is \"capture\")\n"
        "  -d  input driver (see command showdrv for possible drivers), default: file\n"
        "  -m  automatic channel/filename capturing\n"
        "  -v  verbosity level (-v, -vv, -vvv, -vvvv)\n"
        "  -q  quiet output. Only FATAL are shown (supersedes -v)\n\n"

        "Actions:\n"
        "  dump     : creates a sample fingerprint\n"
        "  capture  : tries to capture a sound pattern\n"
        "  showdrv  : show possible input drivers\n\n"

        "[Channel/file]name:\n"
#ifdef HAVE_JACK
        "  file name (- for stdin) or jack channel name\n\n"
#else
        "  file name (- for stdin)\n\n"
#endif
            );
    exit(0);
}

int main (int argc, char *argv[]) {
    char *cfgfile = NULL,    // where treshold coeficients are stored, lengths (human)
         *isource = NULL,    // [channel/file]name
         *samplefile = NULL, // generated by soundpatty
         *idrv = NULL;       // input driver: jack or file

    int quiet = 0;

    if (argc < 2) {
        usage();
    }

    int c;
    action = ACTION_UNDEF;
    while ((c = getopt(argc, argv, "ma:c:s:d:v::q")) != -1)
        switch (c)
        {
            case 'a':
                if (strcmp(optarg, "dump") == 0) {
                    action = ACTION_DUMP;
                } else if (strcmp(optarg, "capture") == 0) {
                    action = ACTION_CAPTURE;
                } else if (strcmp(optarg, "showdrv") == 0) {
                    action = ACTION_SHOWDRV;
                } else {
                    cerr << "Invalid action specified: "<<action<<endl<<endl;
                    usage();
                }
                break;
            case 'c':
                cfgfile = optarg;
                break;
            case 's':
                samplefile = optarg;
                break;
            case 'd':
                idrv = optarg;
                break;
            case 'm':
                channel_hook_way = AUTO;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'v':
                LogLevel = 3;
                if (optarg != NULL) {
                    LogLevel += strlen(optarg);
                }
                break;
        }
    if (optind != argc) {
        isource = argv[optind];
    }

    if (action == ACTION_UNDEF) {
        perror("Action not specified\n\n");
        usage();
    }

    LogLevel = quiet? 1 : LogLevel;

    if (action == ACTION_SHOWDRV) {
            string drivers("file");
#ifdef HAVE_JACK
            drivers += " jack";
#endif
            printf("Possible drivers: %s\n", drivers.c_str());
            exit(0);
    } else if (action == ACTION_CAPTURE) {
        if (samplefile == NULL) {
            perror("Action is catch, but samplefile not specified.");
            usage();
        }
    }


    if (cfgfile == NULL) {
        perror("Config file not specified\n\n");
        usage();
    }
    // ------------------------------------------------------------
    // Input driver is file by default, fix this?
    //
    if (idrv == NULL) {
        LOG_DEBUG("idrv null, making default: file");
        idrv = (char*) malloc(5 * sizeof(char));
        strcpy(idrv, "file");
    }
    // --------------------------------------------------------------------------------
    // Input source must be specified, unless channels are hooked automatically
    //
    if (channel_hook_way == MANUAL and isource == NULL) {
        perror("[channel/file]name not specified\n\n");
        usage();
    }

    LOG_TRACE("cfgfile: %s", cfgfile);
    LOG_TRACE("samplefile: %s", samplefile);
    LOG_TRACE("idrv: %s", idrv);
    LOG_TRACE("isource: %s", isource);

    // ------------------------------------------------------------
    // End of parameter reading, creating input instance
    //

    LOG_DEBUG("Starting to read configs from %s", cfgfile);

    // Will give this parameter list to SoundPatty constructor
    // independent of capture or dump (it will understand it itself)
    sp_params_dump_t    sp_params_dump;
    sp_params_capture_t sp_params_capture;
    void *sp_params = NULL;

    all_cfg_t this_cfg_var = SoundPatty::read_cfg(cfgfile);
    all_cfg_t *this_cfg = &this_cfg_var;

    if (action == ACTION_CAPTURE) {
        LOG_DEBUG("Reading captured values from %s", samplefile);
        sp_params_capture.vals = SoundPatty::read_captured_values(samplefile);
        sp_params_capture.fn = its_over;
        sp_params = (void*)&sp_params_capture;
    } else {
        sp_params = (void*)&sp_params_dump;
    }

    if (channel_hook_way == MANUAL) {
        Input *input = NULL;
        if (strcmp(idrv, "file") == 0) {
            input = new FileInput(isource, this_cfg);
            LOG_INFO("Sox input, input file: %s, created instance", argv[argc-1]);
        } else {
#ifdef HAVE_JACK
            input = new JackInput(isource, this_cfg);
#endif // HAVE_JACK
        }

        LOG_INFO("Creating SoundPatty instance");
        SoundPatty *pat = new SoundPatty(action, input, this_cfg, sp_params);

        LOG_INFO("Starting main SoundPatty loop");
        SoundPatty::go_thread(pat);
        LOG_INFO("SoundPatty main loop completed");
    } else {
        if (strcmp(idrv, "file") == 0) {

            FileInput::monitor_ports(action, isource, this_cfg, sp_params);
            LOG_INFO("Starting FileInput monitor_ports");
        } else {
#ifdef HAVE_JACK
            LOG_INFO("Starting FileInput monitor_ports");
            JackInput::monitor_ports(action, isource, this_cfg, sp_params);
#endif // HAVE_JACK
        }
    }

    exit(0);
}
