#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <locale.h>

typedef int bool;
#define true 1
#define false 0

struct boot_block {
	unsigned char bootstrap[3];
	unsigned char manufacturer[8];
	unsigned short n_bytes_per_block;
	unsigned char n_blocks_per_alloc;
	unsigned short n_reserved_block;
	unsigned char n_fat;
	unsigned short n_root_dirs;
	unsigned short n_blocks_on_disk;
	unsigned char meta_desc;
	unsigned short n_blocks_on_fat;
	unsigned short n_blocks_per_track;
	unsigned short n_heads;
	unsigned int n_hidden_block;
	unsigned int n_blocks_on_disk_true;
	unsigned short physical_drive_nb;
	unsigned char signature;
	unsigned int serial_num;
	unsigned char label[11];
	unsigned char fs_id[8];
};

struct root_dir_entry {
	unsigned char filename[10];
	unsigned char extension[3];
	unsigned char attr;
	unsigned char reserved[10];
	unsigned short time_last_updated;
	unsigned short date_last_updated;
	unsigned short starting_cluster;
	unsigned int file_size;
};

unsigned char* trim_f(unsigned char *f) {
    int i = 7;
    while(i >= 0 && f[i] == ' ') {
        i--;
    }
    
	f[i+1] = '\0';
	return f;
}

unsigned char* trim_e(unsigned char *e) {
    int i = 2;
    while(i >= 0 && e[i] == ' ') {
        i--;
    }
    
	e[i+1] = '\0';
	return e;
}

bool is_valid_entry(struct root_dir_entry r) 
{
	unsigned char sign = r.filename[0];

	if (r.starting_cluster == 0) 
		return false;
	if (sign == 0x00 || sign == 0x20) 
		return false;
	if (sign == 0xE5 || sign == 0x05)
		 return false;
	if ((r.attr & 0x08) == 0x08) 
		return false;
	return true;
}

void read_boot_block(struct boot_block *bb, FILE **f)
{
	fseek(*f, 0, SEEK_SET);
	fread(bb->bootstrap, sizeof(unsigned char), 3, *f);
	fread(bb->manufacturer, sizeof(unsigned char), 8, *f);
	fread(&bb->n_bytes_per_block, sizeof(unsigned char), 2, *f);
	fread(&bb->n_blocks_per_alloc, sizeof(unsigned char), 1, *f);
	fread(&bb->n_reserved_block, sizeof(unsigned char), 2, *f);
	fread(&bb->n_fat, sizeof(unsigned char), 1, *f);
	fread(&bb->n_root_dirs, sizeof(unsigned char), 2, *f);
	fread(&bb->n_blocks_on_disk, sizeof(unsigned char), 2, *f);
	fread(&bb->meta_desc, sizeof(unsigned char), 1, *f);
	fread(&bb->n_blocks_on_fat, sizeof(unsigned char), 2, *f);
	fread(&bb->n_blocks_per_track, sizeof(unsigned char), 2, *f);
	fread(&bb->n_heads, sizeof(unsigned char), 2, *f);
	fread(&bb->n_hidden_block, sizeof(unsigned char), 4, *f);
	fread(&bb->n_blocks_on_disk_true, sizeof(unsigned char), 4, *f);
	fread(&bb->physical_drive_nb, sizeof(unsigned char), 2, *f);
	fread(&bb->signature, sizeof(unsigned char), 1, *f);
	fread(&bb->serial_num, sizeof(unsigned char), 4, *f);
	fread(&bb->label, sizeof(unsigned char), 11, *f);
	fread(&bb->fs_id, sizeof(unsigned char), 8, *f);
}

void read_root_directories(struct root_dir_entry *r, struct boot_block bb, FILE **f)
{
	long pos;
	unsigned int i;
	int root_dir_addr;
	unsigned char bb_sign[2];

	for (i = 0; i < bb.n_root_dirs; i++) {
		root_dir_addr = (bb.n_reserved_block * bb.n_bytes_per_block) 
						+ (bb.n_fat * bb.n_blocks_on_fat * bb.n_bytes_per_block);
		pos = root_dir_addr + (i * 32);
		fseek(*f, pos, SEEK_SET);

		fread(r[i].filename, sizeof(unsigned char), 8, *f);
		fread(r[i].extension, sizeof(unsigned char), 3, *f);
		fread(&r[i].attr, sizeof(unsigned char), 1, *f);
		fread(r[i].reserved, sizeof(unsigned char), 10, *f);
		fread(&r[i].time_last_updated, sizeof(unsigned char), 2, *f);
		fread(&r[i].date_last_updated, sizeof(unsigned char), 2, *f);
		fread(&r[i].starting_cluster, sizeof(unsigned char), 2, *f);
		fread(&r[i].file_size, sizeof(unsigned char), 4, *f);
	}
}

void print_f_time(unsigned short d)
{
	unsigned int hours = 0;
	unsigned int minutes = 0;
	unsigned int seconds = 0;
	char* meridian = "a";

	hours |= (d >> 11) & ((1 << 5) -1);
	minutes |= (d >> 5) & ((1 << 6) -1);
	seconds |= d & ((1 << 5) -1);

	if (hours > 12) {
		hours = hours - 12;
		meridian = "p";
	}

	if (hours == 0) {
		hours = 12;
	}

	printf("%2d:%02d%s", hours, minutes, meridian);
}

void print_f_date(unsigned short d)
{
	unsigned char i;
	
	unsigned int year = 0;
	unsigned int month = 0;
	unsigned int day = 0;

	for (i = 0; i < 7; i++) {
		year |= (d >> 9) & (1 << i);
	}
	year += 1980;

	for (i = 0; i < 4; i++) {
		month |= (d >> 5) & (1 << i);
	}
	for (i = 0; i < 5; i++) {
		day |= d & (1 << i);
	}

	printf("%02d-%02d-%02d  ", month, day, year % 100);
}

void print_boot_block(struct boot_block bb)
{
	printf(" Volume name is %s\n", bb.label);
	printf(" Volume Serial Number is %X\n", bb.serial_num);
	printf("\n");
}

void print_root_directories(struct root_dir_entry *r, unsigned short n_entries)
{
	int i;

	int f_count = 0;
	int f_size = 0;

	for (i = 0; i < n_entries; i++) {
		if (is_valid_entry(r[i])) {
			printf("%-8s %-3s %'12d ", r[i].filename, r[i].extension, r[i].file_size);
			print_f_date(r[i].date_last_updated);
			print_f_time(r[i].time_last_updated);
			printf("\n");

			f_size += r[i].file_size;
			f_count++;
		}
	}

	printf("%9d file(s) %'13d bytes\n\n", f_count, f_size);
}

int main(int argc, char** argv)
{
	struct boot_block bb;
	FILE *f;
	long pos;
	unsigned char bb_sign[2];
	struct root_dir_entry *r_entries;

	setlocale(LC_NUMERIC, "");

	if (argc < 2) {
		printf("Missing image path.\n");
		return 1;
	}

	f = fopen(argv[1], "r");
	if (f == NULL) {
		perror(NULL);
		return 1;
	}

	memset(&bb, 0, sizeof(struct boot_block));
	read_boot_block(&bb, &f);
	print_boot_block(bb);

	r_entries = calloc(sizeof(struct root_dir_entry), bb.n_root_dirs);
	read_root_directories(r_entries, bb, &f);
	print_root_directories(r_entries, bb.n_root_dirs);

	return 0;
}
