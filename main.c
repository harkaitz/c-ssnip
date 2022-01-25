#include "ssnip.h"
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#define SSNIP_TEMPLATE_VAR "SSNIP_TEMPLATE_DIR"
#ifndef SSNIP_TEMPLATE_DIR
#  define SSNIP_TEMPLATE_DIR "templates"
#endif
#define COPYRIGHT_LINE \
    "Donate bitcoin: 1C1ZzDje7vHhF23mxqfcACE8QD4nqxywiV" "\n" \
    "Copyright (c) 2022 Harkaitz Agirre, harkaitz.aguirre@gmail.com" "\n" \
    ""

int main (int _argc, char *_argv[]) {

    char       *progname     = basename(_argv[0]);
    char       *directory_m  = NULL;
    int         argc         = _argc;
    char      **argv         = _argv;
    ssnip      *ssnip        = NULL;
    ssnip_tmpl *tmpl         = NULL;
    const char *s1           = NULL;
    int         retval       = 1;
    int         err          = 0;
    int         opt          = 0;
    
    
    /* Print help. */
    if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        printf("Usage: %s [-lpsiq] [FILES...]" "\n"
               ""                              "\n"
               COPYRIGHT_LINE, progname);
        return 0;
    }

    /* Get template directory. */
    directory_m = strdup((s1 = getenv(SSNIP_TEMPLATE_VAR))?s1:SSNIP_TEMPLATE_DIR);
    
    /* Initialize ssnip. */
    openlog(progname, LOG_PERROR, LOG_USER);
    err = ssnip_create(&ssnip, directory_m);
    if (!err/*err*/) goto cleanup;

    /* Parse options. */
    while ((opt = getopt (argc, argv, "lp:s:iq")) != -1) {
        switch (opt) {
        case 'l':
            ssnip_print_templates(ssnip, stdout);
            break;
        case 'p':
            ssnip_search_tmpl(ssnip, optarg, &tmpl);
            if (!tmpl/*err*/) {
                syslog(LOG_ERR, "Template %s: Not found.", optarg);
                goto cleanup; }
            err = ssnip_tmpl_write(tmpl, NULL, NULL, 1);
            if (!err/*err*/) goto cleanup;
            break;
        case 's':
            err = ssnip_cat(ssnip, optarg, 1, NULL, NULL);
            if (!err/*err*/) goto cleanup;
            break;
        case 'i':
            ssnip->interactive = true;
            break;
        case 'q':
            ssnip->quiet = true;
            break;
        case '?':
        default:
            goto cleanup;
        }
    }

    /* Process files. */
    for (int opt=optind; opt<argc; opt++) {
        err = ssnip_replace(ssnip, argv[opt]);
        if (!err/*err*/) goto cleanup;
    }

    /* Cleanup. */
    retval = 0;
    cleanup:
    ssnip_destroy(ssnip);
    return retval;
}
