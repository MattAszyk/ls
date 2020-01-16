#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#define bool int
#define true 1
#define false 0

const char* version_text = "Beta (2019)\n";
const char* help_text = "Software for list a files on disk.\nSynopsis :\n executable -[flags] [path]\nOptions:\n-l <- show detail of file\n -t <- sort listed files by modification time\n -R <- list files recursevly\n-h show human readable size\n--version <- print version \n --help <-- print manual\n";

/** Funkcja do wyswietlania version/help **/
void print_option(bool version, bool helper)
{
    printf("%s\n", helper ? help_text : version_text);
    exit(0);
}

typedef struct 
{
    char name[256];
    long size;
    int user_id;
    int group_id;
    char perms[13];
    __time_t time;
    bool is_directory;
}file_struct;

struct size
{
	char surfix;
	double size;
};

/** Metoda zwracajaca ścieżkę do podfolderu **/
char *path_generator(char *path, char *file)
{
    int path_length = strlen(path);
    int file_length = strlen(file);
    int complete_length = path_length + file_length + 2;
    char *new_path = (char *) malloc (complete_length * sizeof(char));
    strcpy(new_path,path);
    if(path_length && path[path_length-1] != '/')
    {
        new_path[path_length] = '/';
        strcpy(new_path + path_length +1, file);
        new_path[complete_length-1] = '\0';
    }
    else
    {
        strcpy(new_path + path_length,file);
        new_path[complete_length-1] = '\0';
    }
    return new_path;
}

/** komperator czasu modyfikacji **/
bool cmp_date(const void *a, const void *b)
{
    long a_data = (long)((file_struct*)a)->time;
	long b_data = (long)((file_struct*)b)->time;
	return a_data < b_data;
}


/** wyciagniecie praw dostepow **/
void set_perms(file_struct *container, struct stat *file)
{
    container->perms[0] = S_ISDIR(file->st_mode) ? 'd' : '-';
	container->perms[1] = (file->st_mode & S_IRUSR) ? 'r' : '-';
	container->perms[2] = (file->st_mode & S_IWUSR) ? 'w' : '-';
	container->perms[3] = (file->st_mode & S_IWUSR) ? 'x' : '-';
	container->perms[4] = '-';
	container->perms[5] = (file->st_mode & S_IRGRP) ? 'r' : '-';
	container->perms[6] = (file->st_mode & S_IWGRP) ? 'w' : '-';
	container->perms[7] = (file->st_mode & S_IXGRP) ? 'x' : '-';
	container->perms[8] = '-';
	container->perms[9] = (file->st_mode & S_IROTH) ? 'r' : '-';
	container->perms[10] = (file->st_mode & S_IWOTH) ? 'w' : '-';
	container->perms[11] = (file->st_mode & S_IXOTH) ? 'x' : '-';
	container->perms[12] = '\0';
}

/** struktura zwracajaca konwertacje wag **/
struct size human_readable_size(double bytes)
{
	struct size result;
	char names[6] = {'B', 'K', 'M', 'G', 'T', 'P'};
	int iterator = 0;
	while (bytes > 1024.0 && iterator < 6)
	{
		bytes /= 1024.0;
		iterator++;
	}
	result.size = bytes;
	result.surfix = names[iterator];
	return result;	
}

/** funkcja wyswietlajaca dane pliku **/
void print_file( file_struct *container, bool human_size, bool detail)
{
    if(container->name[0] == '.') return;
    if(detail)
    {
        struct passwd *uname = getpwuid(container->user_id);
        struct group *gname = getgrgid(container->group_id);
        printf("%s ", container->perms);
        printf("%s ", uname->pw_name);
        printf("%s ", gname->gr_name);
        if(human_size)
        {
            struct size format = human_readable_size(container->size);
            printf("%.2f%c ",format.size,format.surfix);
        }
        else
        {
            printf("%ld ", container->size);
        }
        char time_container[256];
		strftime(time_container, 256, "%R %F", localtime(&container->time));
		printf("%s ", time_container);
		printf("%s\n",container->name);
    }
    else
    {
        printf("%s ", container->name);
    }
    
}

/** funkcja zwrajaaca ogolna ilosc plikow w folderze (zwraca ilosc takze pliki niedostepnych do odczytow ) **/
int file_amount_in_path(char *path)
{
    int amount = 0;
    DIR *dir;
    struct dirent *dp;
    if((dir = opendir(path)) == NULL)
    {
        perror("Can not open the directory at file_amount!");
        return 0;
    }
    while((dp = readdir(dir)) != NULL)
    {
        amount++;
    }
    closedir(dir);
    return amount;
}
/**
 * Sposob dzialania:
 * 1. Wczytuje ilosc plikow za pomoca metody file_amount_in_path
 * - jezeli ilosc rowna 2 przerywa dzialanie (lista z [".", ".."])
 * 2. Alokuje tablice struktur file o rozmiarze wczytanym
 * 3. Iteruje do konca plikow w danym katalogu
 * -jezeli plik jest dostepny wpisuje go do struktury oraz zwieksza licznik file_opened (gdyz nie zawsze wszystkie pliki da sie odczytac za pomoca statu)
 * -jezeli plik jest folderem zwieksza licznik ilosci dostepnych folderow
 * 3. Jezeli zaznaczono opcje sortowania to sortuje za pomoca qsort
 * 4. Wypisuje pliki w danym katalogu z zaznaczonymi opcjami
 * 5. Jezeli zaznaczono rekurencje program alokuje tablice samych folderow oraz przepisuje je z tablicy plikow, w innym wypadku zwalnia pamiec i zakoncza prace.
 * 6. Zwalnia liste wszystkich plikow oraz zostawia tablice z folderami.
 * 7. Uruchamia te metode dla sciezek katalogow pozostalych
 * 8. Po wykonaniu zwalnia tablice plikow oraz metoda konczy sie 
 * */
void list_dir(char *path, bool sort, bool human_readable_size, bool detail, bool recursively)
{
    if(recursively)
        printf("\n%s:\n",path);
    DIR *dir;
    struct dirent *dp;
    struct stat stat_buf;
    if((dir = opendir(path)) == NULL)
    {
        perror("Can not open the directory: ");
        return;
    }
    int file_amount = file_amount_in_path(path);
    if(file_amount == 2) return;
    file_struct *file_list = (file_struct*) malloc (sizeof(file_struct) * (file_amount));
    int file_opened = 0;
    int dir_amount = 0;
    int dir_iter = 0;
    int i = 0;
    while((dp = readdir(dir)) != NULL)
    {
        char *full_name = path_generator(path,dp->d_name);
        file_struct *file = file_list+file_opened;
        if(stat(full_name,&stat_buf))
        {
            perror("Cannot open the file!");
            free(full_name);
            continue;
        }
        strcpy(file->name,dp->d_name);
        set_perms(file,&stat_buf);
        file->size = (long)stat_buf.st_size;
        file->user_id = stat_buf.st_uid;
        file->group_id = stat_buf.st_gid;
        file->time = stat_buf.st_mtime;
        file->is_directory = dp->d_type == DT_DIR ? 1 : 0;
        if(file->is_directory == 1) 
            dir_amount++;
        file_opened++;
        free(full_name);
    }    

    if(sort)
        qsort(file_list,file_opened,sizeof(file_struct),cmp_date);
    for(i = 0; i < file_opened; ++i)
        print_file(file_list+i, human_readable_size,detail);
    printf("\n");
    if(dir_amount == 2 || recursively == false)
    {
        free(file_list);
        closedir(dir);
        return;
    }
    file_struct *dir_list = (file_struct*) malloc (sizeof(file_struct) * (dir_amount));
    for(i =  0; i < file_opened; ++i)
    {
        if(file_list[i].is_directory == 1)
        {
            if(strcmp(file_list[i].name,".") == 0 || strcmp(file_list[i].name,"..") == 0) continue;
            dir_list[dir_iter++] = file_list[i];
        }
    }
    free(file_list);           
    closedir(dir);

    for(i = 0; i < dir_iter; ++i)
    {
        char *second_path = path_generator(path, dir_list[i].name);
        list_dir(second_path,sort,human_readable_size,detail,true);
        free(second_path);
    }
    free(dir_list);
}

int main(int argc, char **argv)
{
    /** declaration of flags **/
    int path_id=-1;
    bool sort = false;
    bool recursively = false;
    bool human_size = false;
    bool detail = false;
    int i;
    opterr = 0;
    for(i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i],"--version") == 0)
        {
          print_option(true,false);
        }
        if(strcmp(argv[i],"--help") == 0)
        {
            print_option(false,true);
        }
        if(argv[i][0] != '-')
        {
            path_id = i;
        }
    }
    while ((i = getopt(argc,argv,"lRth")) != -1)
    {
        switch(i)
        {
            case 'l':
                detail = true;
                break;
            case 'R':
                recursively = true;
                break;
            case 't':
                sort = true;
                break;
            case 'h':
                human_size = true;
                break;
        }
    }

    list_dir(path_id == -1 ? "." : argv[path_id], sort,human_size,detail,recursively);

    return 0;
}

