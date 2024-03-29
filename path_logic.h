#ifndef KIV_ZOS_PATH_LOGIC_H
#define KIV_ZOS_PATH_LOGIC_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "path_logic.h"
#include "shell.h"
#include "ntfs_logic.h"
#include "usefull_functions.h"

// Oddelovac slozek
#define PATH_SEPARATOR '/'
// Velikost oddelovace slozek (pri pripadnem pouziti viceznakoveho oddelovace)
#define PATH_SEPARATOR_LENGTH 1

// Maximalni pocet casti v ceste
#define PATH_PARTS_LIMITATION 256

// Maximalni velikost jedne casti cestz
#define PATH_PART_MAX_LENGTH 255

/**
 * Na zaklade aktualniho kontextu shell vytvori retezec
 * obsahujici textovou reprezentaci absolutni cesty pro
 * current working directory
 *
 * @param shell aktualni kontext
 * @return ukazatel na retezec
 */
char *get_current_path(shell *shell);

/**
 * Na zaklade kontextu shell overi zda cesta urcena vstupem
 * path existuje v ramci VFS
 *
 * @param shell
 * @param path
 * @return
 */
int path_exist(shell *shell, char *path);

/**
 * Vrati UID cilove slozky
 *
 * @param shell
 * @param path
 * @return
 */
int path_target_uid(shell *shell, char *path);



#endif //KIV_ZOS_PATH_LOGIC_H
