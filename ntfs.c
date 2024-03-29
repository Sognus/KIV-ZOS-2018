#include <math.h>
#include "ntfs.h"
#include "structure.h"
#include "math.h"


// POZN: MFT je 10% data size


/**
 * Vytvori soubor zadane velikosti
 *
 * @param filename
 * @param bytes
 */
void format_file(char filename[], int bytes)
{
    /* Pokud soubor existuje, smažeme jej */
    if(file_exists(filename))
    {
        // Remove file if exist
        remove(filename);
    }

    // cheat protoze matika je zlo
    FILE *file =  fopen(filename, "wb");

    // Paranoia je mocna - radsi zkontrolovat otevreni souboru
    if(file == NULL)
    {
        printf("ERROR: Soubor %s nelze otevrit/vytvorit!\n", filename);
        return;
    }

    // Vytvoreni souboru urcite velikosti - ALOKACE - THIS IS A CHEAT
    int used = 0;
    while(used < bytes)
    {
        fputc(NTFS_NEUTRAL_CHAR, file);
        used = used + 1;
    }
    fclose(file);

    // Na zacatku je velikost dat stejna jako pozadavana velikost systemu
    int size_left = bytes;
    //printf("DEBUG: Data size (=original size): %d\n", size_left);

    /* Musime odecist velikosti odstatnich casti */
    // odecteni velikosti bootrecordu
    size_left = size_left - sizeof(boot_record);
    //printf("DEBUG: Data size (-boot record tj. %d): %d\n", sizeof(boot_record),size_left);

    // odecteni velikosti MFT
    int mft_size = (int)(floor(bytes * MFT_RATIO));
    size_left = size_left - mft_size;
    //printf("DEBUG: Data size (-mft size tj. %d): %d\n", (int)(ceil(size_left * MFT_RATIO)), size_left);

    // Vypocet poctu clusteru a odpocet volneho mista
    int cluster_count = (int)floor((size_left / MAX_CLUSTER_SIZE));
    int cluster_data_size = (cluster_count * MAX_CLUSTER_SIZE);
    size_left = size_left - cluster_data_size;
    //printf("DEBUG: Data size (-clusters tj. %d ): %d\n",(cluster_count * MAX_CLUSTER_SIZE), size_left);

    /* Vypocet velikosti pro bitmapu */
    // TODO:  mozna upravit bitmapu na bool (char) misto int
    // Pocet bytu, ktery je potreba pro zapsani bitmapy
    int bitmap_bytes_needed = cluster_count * sizeof(int);
    // Pokud je pocet potrebnych byte vetsi nez pocet zbylych bytes
    //printf("DEBUG: Number of clusters = %d\n", cluster_count);
    while(bitmap_bytes_needed > size_left)
    {
        // Musime jeden cluster odebrat
        cluster_count = cluster_count - 1;
        size_left = size_left + MAX_CLUSTER_SIZE;
        //printf("DEBUG: Data size (+ 1 cluster size tj. %d): %d\n", MAX_CLUSTER_SIZE, size_left);
    }

    // Odpocet bytu pouzitych na bitmapu
    size_left = size_left - bitmap_bytes_needed;
    //printf("DEBUG: Data size (-bitmap bytes tj. %d): %d\n", bitmap_bytes_needed, size_left);

    // Pocet bytu potrebnych na zarovnani na urcitou velikost FS
    int alignment_bytes = size_left;

    // Odpocet alignment bytes - v tomto bode by size_left melo byt 0
    size_left = size_left - alignment_bytes;
    //printf("DEBUG: Data size (-alignment bytes tj. %d): %d\n", alignment_bytes, size_left);
    //printf("DEBUG: FINAL number of clusters = %d\n", cluster_count);
    if(size_left != 0)
    {
        printf("ERROR: Chyba ve vypoctu velikosti souboroveho systemu!\n");
        return;
    }

    // Vytvoreni struktury souboru
    create_file(filename, cluster_count, MAX_CLUSTER_SIZE);

}

/*
 * Vytvori soubor s danym pseudo NTFS v pripade, ze neexistuje
 *
 * filename         - nazev souboru
 * cluster_count    - maximalni pocet clusteru pro data
 * cluster_size     - maximalni velikost jednoho clusteru v Byte
 */
void create_file(char filename[], int32_t cluster_count, int32_t cluster_size)
{
    // Deklarace promennych
    FILE *file;
    // TODO:  mozna upravit bitmapu na bool (char) misto int
    int *bitmap = malloc(cluster_count*sizeof(int));

    file = fopen(filename, "r+b");

    // Paranoia je mocna - radsi zkontrolovat otevreni souboru
    if(file == NULL)
    {
        printf("ERROR: Soubor %s nelze otevrit/vytvorit!\n", filename);
        return;
    }

    // Nastaveni ukazatele na zacatek souboru
    fseek(file, 0, SEEK_SET);


    /* VYTVORENI A ZAPIS BOOT RECORDU */

    // Vytvoreni standardniho boot recordu
    boot_record *record = create_standard_boot_record();

    // Uprava standardniho boot recordu (resize)
    boot_record_resize(record, cluster_count, cluster_size);

    // Zapis boot_record do souboru
    fwrite(record, sizeof(boot_record), 1, file);

    /* ----------------------------------------------------------- */
    /* ZAPIS BITMAPY  */

    // Posun na zacatek oblasti pro bitmapu
    fseek(file, record->bitmap_start_address, SEEK_SET);
    // Prvni cluster je využit rootem
    bitmap[0] = true;
    // Vytvoreni prazdne bitmapy
    for(int i = 1; i < cluster_count; i++)
    {
        bitmap[i] = false;
    }
    // Zapis bitmapy
    fwrite(bitmap, sizeof(int), (size_t)cluster_count, file);

    /* --------------------------------------------------------- */
    /* VYTVORENI MFT PRO ROOT */

    // Posun na misto v souboru, kde zacina MFT
    fseek(file, record->mft_start_address, SEEK_SET);

    // Vytvoreni mft itemu
    mft_item *mfti = create_mft_item(1, 1, 1, 1, "/\0", 1);

    // Vytvoreni prvniho fragmentu
    mfti->fragments[0].fragment_start_address = record->data_start_address;
    mfti->fragments[0].fragment_count = 1;

    for(int i = 1; i < MFT_FRAGMENTS_COUNT; i++)
    {
        mfti->fragments[i].fragment_start_address = -1;
        mfti->fragments[i].fragment_count = -1;
    }

    // Zapis mft_item do soubooru
    fwrite(mfti, sizeof(mft_item), 1, file);

    /* ---------------------------------------------------------------------------------------------------- */
    /* ZAPIS ROOTU  */
    fseek(file, record->data_start_address, SEEK_SET);
    int32_t odkaz[2];
    // Prvni dve polozky jsou (1) aktualni slozka (2) nadrazena slozka
    odkaz[0] = 1;
    odkaz[1] = 1;
    fwrite(odkaz, sizeof(int32_t), 2, file);

    // Uvolneni pameti
    fflush(file);
    free(record);
    free(mfti);
    fclose(file);
    free(bitmap);
}

/**
 *  Na zaklade jmena souboru precte boot_record
 *
 * @param filename nazev souboru pro cteni
 * @return boot record or null
 */
boot_record *read_boot_record(char filename[])
{
    // Overeni existence souboru
    if(!file_exists(filename))
    {
        printf("ERROR: Nelze precist boot record ze souboru: Soubor neexistuje!\n");
        return NULL;
    }

    // Otevreni souboru pro cteni
    FILE *file = fopen(filename, "r+b");
    fseek(file, 0, SEEK_SET);

    // Deklarace
    boot_record *record = malloc(sizeof(boot_record));

    // Nacteni
    fread(record, sizeof(boot_record), 1, file);

    // Navrat vysledku
    return record;

}

/**
 * Na zaklade jmena souboru a boot recordu precte bitmapu
 *
 * @param filename soubor, ze ktereho se bude cist
 * @param record zaznam podle ktereho se bude bitmapa načítat
 * @return ukazatel na pole (velikost pole je v boot_record)
 */
int *read_bitmap(char filename[], boot_record *record)
{
    // Overeni existence souboru
    if(!file_exists(filename))
    {
        printf("ERROR: Nelze precist bitmapu ze souboru: Soubor neexistuje!\n");
        return NULL;
    }

    // Overeni nenulovitosti struktury boot record
    if(record == NULL)
    {
        printf("ERROR: Boot record nemuze byt NULL!\n");
        return  NULL;
    }

    // Otevreni souboru
    FILE *file = fopen(filename, "rb");

    // Cluster count = pocet polozek bitmapy
    int32_t bitmap_size = record->cluster_count;
    int32_t bitmap_addr = record->bitmap_start_address;

    // Priprava pameti pro bitmapu
    int *bitmap = malloc(sizeof(int) * bitmap_size);

    // Nastaveni adresy pro cteni
    fseek(file, bitmap_addr, SEEK_SET);

    // Nacteni dat do paměti
    fread(bitmap, sizeof(int), bitmap_size, file);

    // TODO: Kontrola? Ale jak :(

    // UKLID
    fclose(file);

    // Navrat ukazatele na bitmapu
    return bitmap;
}

/**
 * Na zaklade souboru a bootrecordu s nim spjatym precte ze souboru vsechny neprazdne
 * struktury mft_item, vysledek navrati skrz ukazatele
 *
 * @param filename soubor FS
 * @param record zaznam FS
 * @param mft_array ukazatel, pole struktur mft_item
 * @param mft_array_size ukazatel, pocet prvku mft_item
 */
void read_mft_items(char filename[], boot_record *record, mft_item **mft_array, int *mft_array_size)
{
    // Overeni existence souboru
    if(!file_exists(filename))
    {
        printf("ERROR: Nelze precist bitmapu ze souboru: Soubor neexistuje!\n");
        return;
    }

    // Overeni nenulovitosti struktury boot record
    if(record == NULL)
    {
        printf("ERROR: Boot record nemuze byt NULL!\n");
        return;
    }

    // Otevreni souboru
    FILE *file = fopen(filename, "rb");

    // Precteni dulezitych rad z recordu
    int32_t mft_addr_start = record->mft_start_address;
    int32_t mft_addr_end = record->bitmap_start_address - 1;
    int32_t mft_fragments = record->mft_max_fragment_count;

    // Vypocet maximalniho poctu itemu
    size_t mft_item_size = sizeof(mft_item);
    int32_t mft_item_max_count = (int32_t) floor((mft_addr_end - mft_addr_start) / mft_item_size);

    // deklarace pomocnych
    int array_size = mft_item_max_count;
    mft_item *array = malloc(mft_item_max_count * mft_item_size);

    // Nastaveni ukazatele souboru
    fseek(file, mft_addr_start, SEEK_SET);

    // Precteni vsech moznych struktur
    fread(array, mft_item_size, mft_item_max_count, file);

    *mft_array = array;
    *mft_array_size = array_size;
    // TODO: Absolutně pochybuju, že to bude fungovat

    // UKLID
    fclose(file);
}

/**
 * Na zaklade vstupniho parametru overi, zda soubor s danym jmenem existuje
 *
 * @param file_name nazev/cesta souboru k overeni
 * @return existence souboru (0 - neexistuje, 1 existuje)
 */
bool file_exists(const char *fname)
{
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}



/**
 * Vrati velikost souboru v bytech, pokud soubor neexustuje, vrati -1
 *
 * @param filename nazev souboru
 * @return velikost souboru v bytech
 */
int file_size(const char *filename)
{
    if(!file_exists(filename))
    {
        return -1;
    }

    FILE *fp = fopen(filename, "r");

    if(fp == NULL)
    {
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    fclose(fp);

    return size;
}

