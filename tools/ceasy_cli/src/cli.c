#include "cli.h"

#include "asset_compiler.h"
#include "generator.h"
#include "migration.h"
#include "project.h"
#include "sessions.h"
#include "view_compiler.h"

#include <stdio.h>
#include <string.h>

static void cli_help(void) {
    puts("Ceasy\n\nUsage:\n  ceasy generate model <Model> [field:type ...]\n  "
         "ceasy generate migration <name>\n  ceasy db:migrate\n  "
         "ceasy sessions:install\n  ceasy sessions:cleanup\n  "
         "ceasy views:compile\n  ceasy assets:compile");
}

int cli_run(int argc, char **argv) {
    CeasyProject project;

    if (argc == 1 || strcmp(argv[1], "help") == 0 ||
        strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        cli_help();
        return argc == 1 || argc == 2 ? 0 : 1;
    }
    if (!project_find_root(&project)) {
        fprintf(stderr, "error: not inside a cdev/Ceasy project\n");
        return 1;
    }
    if (strcmp(argv[1], "db:migrate") == 0 && argc == 2) {
        return migration_run(&project) ? 0 : 1;
    }
    if (strcmp(argv[1], "sessions:install") == 0 && argc == 2) {
        return sessions_install(&project) ? 0 : 1;
    }
    if (strcmp(argv[1], "sessions:cleanup") == 0 && argc == 2) {
        return sessions_cleanup(&project) ? 0 : 1;
    }
    if (strcmp(argv[1], "views:compile") == 0 && argc == 2) {
        return view_compiler_run(&project) ? 0 : 1;
    }
    if (strcmp(argv[1], "assets:compile") == 0 && argc == 2) {
        return asset_compiler_run(&project) ? 0 : 1;
    }
    if (strcmp(argv[1], "generate") == 0 && argc >= 4 &&
        strcmp(argv[2], "model") == 0) {
        return generator_model(&project, argv[3], argc - 4, argv + 4) ? 0 : 1;
    }
    if (strcmp(argv[1], "generate") == 0 && argc == 4 &&
        strcmp(argv[2], "migration") == 0) {
        return generator_migration(&project, argv[3]) ? 0 : 1;
    }
    fprintf(stderr, "error: unknown or invalid command\n");
    cli_help();
    return 1;
}
