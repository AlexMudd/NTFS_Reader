#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#pragma pack(1)

//#define DEBUG_ON


typedef struct {
    uint8_t jmp[3];
    char NTFS_sign[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t non_used1[7];
    uint8_t media_descriptor;
    uint16_t unused2;
    uint16_t sectors_per_track;
    uint16_t num_of_heads;
    uint32_t hidden_sectors;
    uint64_t unused3;
    uint64_t total_sectors;
    uint64_t MFT_cluster_number;
} BPB;

typedef struct {
    uint32_t signature;
    uint16_t mark_offset;
    uint16_t mark_count;
    uint64_t id_transaction;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t first_attr_offset;
    uint16_t flag;
    uint32_t real_record_size;
    uint32_t allocated_record_size;
    uint64_t base_file_record;
    uint16_t id_next_attr;
    uint16_t reserved;
    uint32_t cur_file_record_num;
    uint16_t other[488];
} RECORD;

typedef struct {
    uint32_t attr_type;
    uint32_t len_including_header;
    uint8_t non_res_flag;
    uint8_t name_len;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t id_attr;
    uint32_t attr_len;
    uint16_t attr_offset;
    uint8_t indexed_flag;
    uint8_t reserved;
} ATTRIBUTE_HEADER_RES;

typedef struct {
    uint32_t attr_type;
    uint32_t len_including_header;
    uint8_t non_res_flag;
    uint8_t name_len;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t id_attr;
    uint64_t start_cluster;
    uint64_t end_cluster;
    uint16_t series_offset;
    uint16_t comp_unit_size;
    uint32_t padding;
    uint64_t allocated_attr_size;
    uint64_t real_attr_size;
    uint64_t data_size_of_stream;
} ATTRIBUTE_HEADER_NON_RES;

typedef struct {
    uint64_t ref_to_par_dir;
    uint64_t file_creation;
    uint64_t file_altered;
    uint64_t mft_changed;
    uint64_t file_read;
    uint64_t allocated_file_size;
    uint64_t real_file_size;
    uint32_t flags;
    uint32_t non_used;
    uint8_t filename_len;
    uint8_t filename_namespace;
} FILE_NAME_ATTR;

typedef struct {
    uint32_t INDX;
    uint16_t markers_offset;
    uint16_t markers_count;
    uint64_t LogFile_seq_num;
    uint64_t VCN;
    uint32_t first_entry_offset;
    uint32_t all_entries_size;
    uint32_t allocated_node_size;
    uint8_t non_leaf_flag;
    uint8_t padding[3];
} INDEX_RECORD_HEADER; //(0xA0)

typedef struct {
    uint64_t file_ref;
    uint16_t len;
    uint16_t stream_len;
    uint8_t flags;
} INDEX_ENTRY_HEADER;

typedef struct {
    uint32_t attr_type;
    uint32_t collaption_rule;
    uint32_t bytes_per_index_record;
    uint8_t clusters_per_index_record;
    uint8_t shit[3];
    uint32_t first_entry_offset;
    uint32_t all_entries_size;
    uint32_t allocated_node_size;
    uint8_t non_leaf_flag;
    uint8_t padding[3];
}INDEX_ROOT_HEADER; //(0x90)

void ClearUpdateSequence(RECORD rec, BPB boot, int rec_offset, FILE *file) {
    int save_offset = ftell(file);
    fseek(file, rec_offset, SEEK_SET);

    for (int i = 1; i < rec.mark_count; i++)
    {
        fseek(file, boot.bytes_per_sector - 2, SEEK_CUR);
        fwrite(&rec.other[i], 2, 1, file);
    }

    fseek(file, save_offset, SEEK_SET);
}

void SetUpdateSequence(RECORD rec, BPB boot, int rec_offset, FILE *file) {
    int save_offset = ftell(file);
    fseek(file, rec_offset, SEEK_SET);

    for (int i = 1; i < rec.mark_count; i++)
    {
        fseek(file, boot.bytes_per_sector - 2, SEEK_CUR);
        fwrite(&rec.other[0], 2, 1, file);
    }

    fseek(file, save_offset, SEEK_SET);
}

void ClearUpdateSequenceINDX(INDEX_RECORD_HEADER rec, BPB boot, FILE *file) {
    int save_offset = ftell(file);
    fseek(file, 0, SEEK_SET);

    for (int i = 1; i < rec.markers_count; i++)
    {
        fseek(file, rec.markers_offset + i * 2, SEEK_SET);
        uint16_t xchng;
        fread(&xchng, 2, 1, file);
        fseek(file, boot.bytes_per_sector * i - 2, SEEK_SET);
        fwrite(&xchng, 2, 1, file);
    }

    fseek(file, save_offset, SEEK_SET);
}

void printDataNonRes(FILE *output, FILE *disk, int ser_offset, int16_t bytes_per_sector, uint8_t sectors_per_cluster, uint64_t attr_size) {
    int bytes_read = 0;
    int bytes_to_skip = 0;
    uint8_t first_byte;

    uint8_t addr_size;
    uint8_t count_size;

    __uint128_t count = 0;
    __uint128_t start_clust = 0;

    int save_offset = ftell(disk);

    FILE *tmp = fopen("tmp", "wb+");

    while (bytes_read != attr_size)
    {
        fseek(disk, ser_offset + bytes_to_skip, SEEK_SET);
        fread(&first_byte, 1, 1, disk);

        count_size = first_byte & 0x0F;
        addr_size = (first_byte & 0xF0) / 0x10;

        if (addr_size == 0)
        {
            break;
        }

        fread(&count, count_size, 1, disk);
        fread(&start_clust, addr_size, 1, disk);

        bytes_to_skip += count_size + addr_size + 1;

        fseek(disk, start_clust * bytes_per_sector * sectors_per_cluster, SEEK_SET);
        uint8_t *buffer = (uint8_t *)malloc(bytes_per_sector * sectors_per_cluster);
        for (int i = 0; i < count; i++)
        {
            for (int j = 0; j < bytes_per_sector * sectors_per_cluster; j++)
            {
                fread(&buffer[j], 1, 1, disk);
            }
            for (int j = 0; j < bytes_per_sector * sectors_per_cluster; j++)
            {
                fwrite(&buffer[j], 1, 1, tmp);
            }

            bytes_read += bytes_per_sector * sectors_per_cluster;
        }
        free(buffer);
    }
    rewind(tmp);
    rewind(output);
    uint8_t buff;
    for (int i = 0; i < attr_size; i++)
    {
        fread(&buff, 1, 1, tmp);
        fwrite(&buff, 1, 1, output);
    }
    remove("tmp");
    fseek(disk, save_offset, SEEK_SET);
}

int isDir(RECORD rec, int rec_offset, FILE *disk) {
    int save_offset = ftell(disk);

    ATTRIBUTE_HEADER_RES attr_res;
    fseek(disk, rec_offset + rec.first_attr_offset, SEEK_SET);
    fread(&attr_res, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
    int attr_skip = 0;
    while (attr_res.attr_type != 0xFFFFFFFF && attr_res.attr_type != 0)
    {
        if (attr_res.attr_type == 0x30)
        {
            FILE_NAME_ATTR name;
            fread(&name, sizeof(FILE_NAME_ATTR), 1, disk);
            fseek(disk, save_offset, SEEK_SET);
            return (name.flags >= 0x10000000);
        }
        else
        {
            attr_skip += attr_res.len_including_header;
            fseek(disk, rec_offset + rec.first_attr_offset + attr_skip, SEEK_SET);
            fread(&attr_res, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
        }
    }
    fseek(disk, save_offset, SEEK_SET);
}

void CleanString(char* string){
    for(int i = 0; i < 255; i++) {
        string[i] = '\0';
    }
}

int FindAttr(FILE* disk, int record_offset, int first_attr_offset, int attr_type_to_find){
    int save_offset = ftell(disk);
    ATTRIBUTE_HEADER_RES attr_res;

    fseek(disk, record_offset + first_attr_offset, SEEK_SET);
    fread(&attr_res, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
    int attr_skip = 0;
    
    while (attr_res.attr_type != 0xFFFFFFFF && attr_res.attr_type != 0){
        if (attr_res.attr_type == attr_type_to_find) {
            fseek(disk, save_offset, SEEK_SET);
            return (record_offset + first_attr_offset + attr_skip);
        }
        attr_skip += attr_res.len_including_header;
        fseek(disk, record_offset + first_attr_offset + attr_skip, SEEK_SET);
        fread(&attr_res, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
    }
    fseek(disk, save_offset, SEEK_SET);
    return -1;
}

int isRes(FILE* disk, int attribute_offset){
    int save_offset = ftell(disk);
    fseek(disk, attribute_offset + 8, SEEK_SET);
    uint8_t non_res;
    fread(&non_res, 1, 1, disk);
    fseek(disk, save_offset, SEEK_SET);
    return (non_res ? 0 : 1);
}

void CheckRec(BPB boot, FILE *disk, char *cur_path, char *name_to_find, int rec_offset, char *dir_name){
    if(dir_name[0] == '$'){
        return;
    }

    RECORD rec;

    fseek(disk, rec_offset, SEEK_SET);
    fread(&rec, sizeof(RECORD), 1, disk);

    ClearUpdateSequence(rec, boot, rec_offset, disk);

    if (isDir(rec, rec_offset, disk)){
        ATTRIBUTE_HEADER_RES attr_0x90;
        ATTRIBUTE_HEADER_NON_RES attr_0xA0;

        int attr_0x90_offset = FindAttr(disk, rec_offset, rec.first_attr_offset, 0x90);
        int attr_0xA0_offset = FindAttr(disk, rec_offset, rec.first_attr_offset, 0xA0);

        if(attr_0x90_offset != -1 && strcmp(cur_path, "./")){
            fseek(disk, attr_0x90_offset, SEEK_SET);
            fread(&attr_0x90, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);

            fseek(disk, attr_0x90_offset + sizeof(ATTRIBUTE_HEADER_RES) + attr_0x90.name_len*2, SEEK_SET);
            INDEX_ROOT_HEADER index_root;
            fread(&index_root, sizeof(INDEX_ROOT_HEADER), 1, disk);

            int first_entry_offset = attr_0x90_offset + sizeof(ATTRIBUTE_HEADER_RES) + attr_0x90.name_len*2 + 16 + index_root.first_entry_offset;
            fseek(disk, first_entry_offset, SEEK_SET);
            INDEX_ENTRY_HEADER entry;
            fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, disk);

            
            int skip_entry = 0;
            while (entry.file_ref != 0){
                uint64_t index_rec_number = entry.file_ref & 0x00FFFFFF;
                uint64_t index_rec_offset = boot.bytes_per_sector * boot.sectors_per_cluster * boot.MFT_cluster_number + index_rec_number * sizeof(RECORD);
                fseek(disk, index_rec_offset, SEEK_SET);
                RECORD rec;
                fread(&rec, sizeof(RECORD), 1, disk);
                if (isDir(rec, index_rec_offset, disk)){
                    char name[255];
                    CleanString(name);
                    ATTRIBUTE_HEADER_RES attr;
                    FILE_NAME_ATTR filename_attr;

                    int filename_attr_offset = FindAttr(disk, index_rec_offset, rec.first_attr_offset, 0x30);
                    fseek(disk, filename_attr_offset, SEEK_SET);
                    fread(&attr, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
                    fread(&filename_attr, sizeof(FILE_NAME_ATTR), 1, disk);

                    for (int i = 0; i < filename_attr.filename_len; i++) {
                        fread(&name[i], 1, 1, disk);
                        fseek(disk, 1, SEEK_CUR);
                    }

                    #ifdef DEBUG_ON
                        printf("FOLDER: %s offset: %x\n", name, index_rec_offset);
                    #endif

                    //////////////////костыль??????
                    if(!strcmp(name, ".") || name[0] == '$'){
                        skip_entry += entry.len;
                        fseek(disk, first_entry_offset + skip_entry, SEEK_SET);
                        fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, disk);
                        continue;
                    }
                    /////////////////////////////

                    char newPath[255];
                    CleanString(newPath);

                    strcpy(newPath, cur_path);
                    strcat(newPath, name);
                    strcat(newPath, "/");

                    CheckRec(boot, disk, newPath, name_to_find, index_rec_offset, name);
                    skip_entry += entry.len;
                    fseek(disk, first_entry_offset + skip_entry, SEEK_SET);
                    fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, disk);
                }
                
                else{
                    fseek(disk, index_rec_offset, SEEK_SET);
                    CheckRec(boot, disk, cur_path, name_to_find, index_rec_offset, dir_name);
                    skip_entry += entry.len;
                    fseek(disk, first_entry_offset + skip_entry, SEEK_SET);
                    fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, disk);
                }
            }

        }
        if(attr_0xA0_offset != -1){
            fseek(disk, attr_0xA0_offset, SEEK_SET);
            fread(&attr_0xA0, sizeof(ATTRIBUTE_HEADER_NON_RES), 1, disk);

            int ser_offset = attr_0xA0_offset + attr_0xA0.name_offset + attr_0xA0.name_len * 2;
            int bytes_read = 0;
            int bytes_to_skip = 0;
            uint8_t first_byte;

            uint8_t addr_size;
            uint8_t count_size;

            __uint128_t count = 0;
            __uint128_t start_clust = 0;

            uint8_t *buffer = (uint8_t *)malloc(boot.bytes_per_sector * boot.sectors_per_cluster * attr_0xA0.allocated_attr_size);

            FILE *index = fopen(dir_name, "wb+");
            
            //заполняем файл index папки
            while(bytes_read != attr_0xA0.allocated_attr_size) {
                fseek(disk, ser_offset + bytes_to_skip, SEEK_SET);
                fread(&first_byte, 1, 1, disk);

                count_size = first_byte & 0x0F;
                addr_size = (first_byte & 0xF0) / 0x10;
                if(addr_size == 0) {break;}

                fread(&count, count_size, 1, disk);
                fread(&start_clust, addr_size, 1, disk);

                bytes_to_skip += count_size + addr_size + 1;

                fseek(disk, start_clust * boot.bytes_per_sector * boot.sectors_per_cluster, SEEK_SET);

                for (int i = 0; i < count; i++) {
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++) {
                        fread(&buffer[j], 1, 1, disk);
                    }
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++) {
                        fwrite(&buffer[j], 1, 1, index);
                    }
                    bytes_read += boot.bytes_per_sector * boot.sectors_per_cluster;
                }
            }
            free(buffer);

            //Работа с index
            rewind(index);
            INDEX_RECORD_HEADER cur_index;
            INDEX_ENTRY_HEADER entry;

            fread(&cur_index, sizeof(INDEX_RECORD_HEADER), 1, index);
            ClearUpdateSequenceINDX(cur_index, boot, index);

            fseek(index, 0x18 + cur_index.first_entry_offset, SEEK_SET);
            fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, index);

            int skip_entry = 0;
            while (entry.file_ref != 0){ ///с условием чтото
                uint64_t index_rec_number = entry.file_ref & 0x00FFFFFF;
                uint64_t index_rec_offset = boot.bytes_per_sector * boot.sectors_per_cluster * boot.MFT_cluster_number + index_rec_number * sizeof(RECORD);
                fseek(disk, index_rec_offset, SEEK_SET);
                RECORD rec;
                fread(&rec, sizeof(RECORD), 1, disk);
                
                //подготовка нового пути для проверки папки
                if (isDir(rec, index_rec_offset, disk)){
                    char name[255];
                    CleanString(name);
                    ATTRIBUTE_HEADER_RES attr;
                    FILE_NAME_ATTR filename_attr;

                    int filename_attr_offset = FindAttr(disk, index_rec_offset, rec.first_attr_offset, 0x30);
                    fseek(disk, filename_attr_offset, SEEK_SET);
                    fread(&attr, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
                    fread(&filename_attr, sizeof(FILE_NAME_ATTR), 1, disk);

                    for (int i = 0; i < filename_attr.filename_len; i++) {
                        fread(&name[i], 1, 1, disk);
                        fseek(disk, 1, SEEK_CUR);
                    }

                    #ifdef DEBUG_ON
                        printf("FOLDER: %s offset: %x\n", name, index_rec_offset);
                    #endif

                    //////////////////костыль??????
                    if(!strcmp(name, ".") || name[0] == '$'){
                        skip_entry += entry.len;
                        fseek(index, 0x18 + cur_index.first_entry_offset + skip_entry, SEEK_SET);
                        fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, index);
                        continue;
                    }
                    /////////////////////////////

                    char newPath[255];
                    CleanString(newPath);

                    strcpy(newPath, cur_path);
                    strcat(newPath, name);
                    strcat(newPath, "/");

                    CheckRec(boot, disk, newPath, name_to_find, index_rec_offset, name);
                    skip_entry += entry.len;
                    fseek(index, 0x18 + cur_index.first_entry_offset + skip_entry, SEEK_SET);
                    fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, index);
                }
                else{
                    CheckRec(boot, disk, cur_path, name_to_find, index_rec_offset, dir_name);
                    skip_entry += entry.len;
                    fseek(index, 0x18 + cur_index.first_entry_offset + skip_entry, SEEK_SET);
                    fread(&entry, sizeof(INDEX_ENTRY_HEADER), 1, index);
                }


            }
            fclose(index);
            remove(dir_name);
        }
    }


    else{
        ATTRIBUTE_HEADER_RES attr;
        FILE_NAME_ATTR filename_attr;
        int name_attr_offset = FindAttr(disk, rec_offset, rec.first_attr_offset, 0x30);
        fseek(disk, name_attr_offset, SEEK_SET);
        fread(&attr, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);
        fread(&filename_attr, sizeof(FILE_NAME_ATTR), 1, disk);

        char filename[255];
        CleanString(filename);

        for (int i = 0; i < filename_attr.filename_len; i++) {
            fread(&filename[i], 1, 1, disk);
            fseek(disk, 1, SEEK_CUR);
        }

        #ifdef DEBUG_ON
            printf("%s\n", filename);
        #endif

        char fullname[255];
        CleanString(fullname);
        strcpy(fullname, cur_path);
        strcat(fullname, filename);

        if (!strcmp(fullname, name_to_find)){
            int data_attr_offset = FindAttr(disk, rec_offset, rec.first_attr_offset, 0x80);
            FILE *output = fopen(filename, "wb+");
            //Resident
            if(isRes(disk, data_attr_offset)){
                ATTRIBUTE_HEADER_RES data_attr;
                fseek(disk, data_attr_offset, SEEK_SET);
                fread(&data_attr, sizeof(ATTRIBUTE_HEADER_RES), 1, disk);

                int file_size = data_attr.attr_len;
                fseek(disk, data_attr_offset + data_attr.attr_offset, SEEK_SET);
                uint8_t buf;
                 for (int k = 0; k < file_size; k++) {
                    fread(&buf, 1, 1, disk);
                    fwrite(&buf, 1, 1, output);
                }               
                fclose(output);
                printf("File has been found\n");
            }
            //Non-resident
            else{
                ATTRIBUTE_HEADER_NON_RES data_attr;
                fseek(disk, data_attr_offset, SEEK_SET);
                fread(&data_attr, sizeof(ATTRIBUTE_HEADER_NON_RES), 1, disk);

                int ser_offset = data_attr_offset + data_attr.series_offset;
                printDataNonRes(output, disk, ser_offset, boot.bytes_per_sector, boot.sectors_per_cluster, data_attr.real_attr_size);
                fclose(output);
                printf("File has been found\n");
            }  
        }
    }
    SetUpdateSequence(rec, boot, rec_offset, disk);
}

int main() {
    BPB boot;
    RECORD rec;
    char nonParsedFileToFind[255];
    CleanString(nonParsedFileToFind);
    char FileToFind[255];
    CleanString(FileToFind);
    char diskPath[255];
    CleanString(diskPath);

    printf("Enter path to image: ");
    scanf("%s", &diskPath);

    FILE* disk = fopen(diskPath, "rb+");
    if(!disk){
        printf("Can't open the image!\n");
        return 0;
    }
    fread(&boot, sizeof(BPB), 1, disk);
    if (strcmp(boot.NTFS_sign, "NTFS    ")) {
        printf("It is not NTFS image\n");
        return 0;
    }

    printf("Print full path of file you want to find (with ./ | Example: ./fold1/fold2/fortask ): ");
    scanf("%s", &nonParsedFileToFind);

    if(nonParsedFileToFind[0] != '.' || nonParsedFileToFind[1] != '/'){
        strcpy(FileToFind, "./");
        strcat(FileToFind, nonParsedFileToFind);
    }
    else{
        strcpy(FileToFind, nonParsedFileToFind);
    }

    int MFT_offset = boot.bytes_per_sector * boot.sectors_per_cluster * boot.MFT_cluster_number;

    int ROOT_offset = MFT_offset + 5 * sizeof(RECORD);

    CheckRec(boot, disk, "./", FileToFind, ROOT_offset, "root");

    return 0;
}
