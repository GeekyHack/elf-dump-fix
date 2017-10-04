#define _CRT_SECURE_NO_WARNINGS
#include "fix.h"

static const char* g_str = "..dynsym..dynstr..hash..rel.dyn..rel.plt..plt..text@.ARM.extab..ARM.exidx..fini_array..init_array..dynamic..got..data..bss..shstrtab\0";
static const char* g_strtabcontent = "\0.dynsym\0.dynstr\0.hash\0.rel.dyn\0.rel.plt\0.plt\0.text@.ARM.extab\0.ARM.exidx\0.fini_array\0.init_array\0.dynamic\0.got\0.data\0.bss\0.shstrtab\0";

//段表项
static Elf32_Shdr g_shdr[SHDRS] = { 0 };

void get_elf_header(Elf32_Ehdr *pehdr, const char *buffer)
{
	int header_len = sizeof(Elf32_Ehdr);
	memcpy(pehdr, (void*)buffer, header_len);
}

long get_file_len(FILE* p)
{
	fseek (p, 0, SEEK_END);
	long fsize = ftell (p);
	rewind (p);
	return fsize;
}


void regen_section_header(Elf32_Phdr *phdr, const Elf32_Ehdr *pehdr, const char *buffer)
{
	Elf32_Dyn* dyn = NULL;
	Elf32_Phdr load = { 0 };
	
	int ph_num = pehdr->e_phnum;
	int dyn_size = 0, dyn_off = 0;
	int nbucket = 0, nchain = 0;
	int i = 0;
	
	for(;i < ph_num;i++) {
		//段在文件中的偏移修正，因为从内存dump出来的文件偏移就是在内存的偏移
		phdr[i].p_offset =  phdr[i].p_vaddr;
		Elf32_Word p_type = phdr[i].p_type;
		if(p_type == PT_DYNAMIC) {
			//动态表，动态表包括很多项，找到动态表位置可以恢复大部分结构
			g_shdr[DYNAMIC].sh_name = strstr(g_str, ".dynamic") - g_str;
			g_shdr[DYNAMIC].sh_type = SHT_DYNAMIC;
			g_shdr[DYNAMIC].sh_flags = SHF_WRITE | SHF_ALLOC;
			g_shdr[DYNAMIC].sh_addr = phdr[i].p_vaddr;
			g_shdr[DYNAMIC].sh_offset = phdr[i].p_offset;
			g_shdr[DYNAMIC].sh_size = phdr[i].p_filesz;
			g_shdr[DYNAMIC].sh_link = 2;
			g_shdr[DYNAMIC].sh_info = 0;
			g_shdr[DYNAMIC].sh_addralign = 4;
			g_shdr[DYNAMIC].sh_entsize = 8;
			dyn_size = phdr[i].p_filesz;
			dyn_off = phdr[i].p_offset;
			continue;
		}
		
		if(phdr[i].p_type == PT_LOPROC || phdr[i].p_type == PT_LOPROC + 1) {
			g_shdr[ARMEXIDX].sh_name = strstr(g_str, ".ARM.exidx") - g_str;
			g_shdr[ARMEXIDX].sh_type = SHT_LOPROC;
			g_shdr[ARMEXIDX].sh_flags = SHF_ALLOC;
			g_shdr[ARMEXIDX].sh_addr = phdr[i].p_vaddr;
			g_shdr[ARMEXIDX].sh_offset = phdr[i].p_offset;
			g_shdr[ARMEXIDX].sh_size = phdr[i].p_filesz;
			g_shdr[ARMEXIDX].sh_link = 7;
			g_shdr[ARMEXIDX].sh_info = 0;
			g_shdr[ARMEXIDX].sh_addralign = 4;
			g_shdr[ARMEXIDX].sh_entsize = 8;
			continue;
		}
	}
	
	dyn = (Elf32_Dyn*)malloc(dyn_size);
	memcpy(dyn,buffer+dyn_off,dyn_size);
	i = 0;
	int n = dyn_size / sizeof(Elf32_Dyn);
	
	Elf32_Word __global_offset_table = 0;
	for (; i < n; i++) {
		int tag = dyn[i].d_tag;
		switch (tag) {
			case DT_SYMTAB:
				g_shdr[DYNSYM].sh_name = strstr(g_str, ".dynsym") - g_str;
				g_shdr[DYNSYM].sh_type = SHT_DYNSYM;
				g_shdr[DYNSYM].sh_flags = SHF_ALLOC;
				g_shdr[DYNSYM].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[DYNSYM].sh_offset = dyn[i].d_un.d_ptr;
				g_shdr[DYNSYM].sh_link = 2;
				g_shdr[DYNSYM].sh_info = 1;
				g_shdr[DYNSYM].sh_addralign = 4;
				g_shdr[DYNSYM].sh_entsize = 16;
				break;
				
			case DT_STRTAB:
				g_shdr[DYNSTR].sh_name = strstr(g_str, ".dynstr") - g_str;
				g_shdr[DYNSTR].sh_type = SHT_STRTAB;
				g_shdr[DYNSTR].sh_flags = SHF_ALLOC;
				g_shdr[DYNSTR].sh_offset = dyn[i].d_un.d_ptr;
				g_shdr[DYNSTR].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[DYNSTR].sh_addralign = 1;
				g_shdr[DYNSTR].sh_entsize = 0;
				break;
				
			case DT_HASH:
				g_shdr[HASH].sh_name = strstr(g_str, ".hash") - g_str;
				g_shdr[HASH].sh_type = SHT_HASH;
				g_shdr[HASH].sh_flags = SHF_ALLOC;
				g_shdr[HASH].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[HASH].sh_offset = dyn[i].d_un.d_ptr;
				memcpy(&nbucket, buffer + g_shdr[HASH].sh_offset, 4);
				memcpy(&nchain, buffer + g_shdr[HASH].sh_offset + 4, 4);
				g_shdr[HASH].sh_size = (nbucket + nchain + 2) * sizeof(int);
				g_shdr[HASH].sh_link = 4;
				g_shdr[HASH].sh_info = 1;
				g_shdr[HASH].sh_addralign = 4;
				g_shdr[HASH].sh_entsize = 4;
				break;
				
			case DT_REL:
				g_shdr[RELDYN].sh_name = strstr(g_str, ".rel.dyn") - g_str;
				g_shdr[RELDYN].sh_type = SHT_REL;
				g_shdr[RELDYN].sh_flags = SHF_ALLOC;
				g_shdr[RELDYN].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[RELDYN].sh_offset = dyn[i].d_un.d_ptr;
				g_shdr[RELDYN].sh_link = 4;
				g_shdr[RELDYN].sh_info = 0;
				g_shdr[RELDYN].sh_addralign = 4;
				g_shdr[RELDYN].sh_entsize = 8;
				break;
				
			case DT_JMPREL:
				g_shdr[RELPLT].sh_name = strstr(g_str, ".rel.plt") - g_str;
				g_shdr[RELPLT].sh_type = SHT_REL;
				g_shdr[RELPLT].sh_flags = SHF_ALLOC;
				g_shdr[RELPLT].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[RELPLT].sh_offset = dyn[i].d_un.d_ptr;
				g_shdr[RELPLT].sh_link = 1;
				g_shdr[RELPLT].sh_info = 6;
				g_shdr[RELPLT].sh_addralign = 4;
				g_shdr[RELPLT].sh_entsize = 8;
				break;
				
			case DT_PLTRELSZ:
				g_shdr[RELPLT].sh_size = dyn[i].d_un.d_val;
				break;
				
			case DT_FINI:
				g_shdr[FINIARRAY].sh_name = strstr(g_str, ".fini_array") - g_str;
				g_shdr[FINIARRAY].sh_type = 15;
				g_shdr[FINIARRAY].sh_flags = SHF_WRITE | SHF_ALLOC;
				g_shdr[FINIARRAY].sh_offset = dyn[i].d_un.d_ptr - 0x1000;
				g_shdr[FINIARRAY].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[FINIARRAY].sh_addralign = 4;
				g_shdr[FINIARRAY].sh_entsize = 0;
				break;
				
			case DT_INIT:
				g_shdr[INITARRAY].sh_name = strstr(g_str, ".init_array") - g_str;
				g_shdr[INITARRAY].sh_type = 14;
				g_shdr[INITARRAY].sh_flags = SHF_WRITE | SHF_ALLOC;
				g_shdr[INITARRAY].sh_offset = dyn[i].d_un.d_ptr - 0x1000;
				g_shdr[INITARRAY].sh_addr = dyn[i].d_un.d_ptr;
				g_shdr[INITARRAY].sh_addralign = 4;
				g_shdr[INITARRAY].sh_entsize = 0;
				break;
				
			case DT_RELSZ:
				g_shdr[RELDYN].sh_size = dyn[i].d_un.d_val;
				break;
				
			case DT_STRSZ:
				g_shdr[DYNSTR].sh_size = dyn[i].d_un.d_val;
				break;
				
			case DT_PLTGOT:
				g_shdr[GOT].sh_name = strstr(g_str, ".got") - g_str;
				g_shdr[GOT].sh_type = SHT_PROGBITS;
				g_shdr[GOT].sh_flags = SHF_WRITE | SHF_ALLOC;
				//TODO:这里基于假设.got一定在.dynamic段之后，并不可靠，王者荣耀libGameCore.so就是例外
				g_shdr[GOT].sh_addr = g_shdr[DYNAMIC].sh_addr + g_shdr[DYNAMIC].sh_size;
				g_shdr[GOT].sh_offset = g_shdr[GOT].sh_addr - 0x1000;
				__global_offset_table = dyn[i].d_un.d_ptr;
				g_shdr[GOT].sh_addralign = 4;
				break;
		}
	}
	free(dyn);
	if (__global_offset_table)
	{
		Elf32_Word gotBase = g_shdr[GOT].sh_addr;
		Elf32_Word gotEnd = __global_offset_table + 4 * (g_shdr[RELPLT].sh_size) / sizeof(Elf32_Rel) + 3 * sizeof(int);
		
		//RELPLT有多少个成员__global_offset_table里面就有多少个成员
		if (gotEnd > gotBase)
		{
			g_shdr[GOT].sh_size = gotEnd - gotBase;
			//假定DATA紧接着GOT
			//此时GOT才是可靠的值，才能用来修复
			g_shdr[DATA].sh_name = strstr(g_str, ".data") - g_str;
			g_shdr[DATA].sh_type = SHT_PROGBITS;
			g_shdr[DATA].sh_flags = SHF_WRITE | SHF_ALLOC;
			g_shdr[DATA].sh_addr = g_shdr[GOT].sh_addr + g_shdr[GOT].sh_size;
			g_shdr[DATA].sh_offset = g_shdr[DATA].sh_addr - 0x1000;
			g_shdr[DATA].sh_size = load.p_vaddr + load.p_filesz - g_shdr[DATA].sh_addr;
			g_shdr[DATA].sh_addralign = 4;
			g_shdr[GOT].sh_size = g_shdr[DATA].sh_offset - g_shdr[GOT].sh_offset;
		}
		else
		{
			//.got紧接着.dynamic的假设不成立
			//无法修复GOT，全部清零，以免影响ida分析
			memset(&g_shdr[GOT], 0, sizeof(Elf32_Shdr));
		}
	}
	
	//STRTAB地址 - SYMTAB地址 = SYMTAB大小
	g_shdr[DYNSYM].sh_size = g_shdr[DYNSTR].sh_addr - g_shdr[DYNSYM].sh_addr;
	
	g_shdr[FINIARRAY].sh_size = g_shdr[INITARRAY].sh_addr - g_shdr[FINIARRAY].sh_addr;
	g_shdr[INITARRAY].sh_size = g_shdr[DYNAMIC].sh_addr - g_shdr[INITARRAY].sh_addr;
	
	g_shdr[PLT].sh_name = strstr(g_str, ".plt") - g_str;
	g_shdr[PLT].sh_type = SHT_PROGBITS;
	g_shdr[PLT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	g_shdr[PLT].sh_addr = g_shdr[RELPLT].sh_addr + g_shdr[RELPLT].sh_size;
	g_shdr[PLT].sh_offset = g_shdr[PLT].sh_addr;
	g_shdr[PLT].sh_size = (20 + 12 * (g_shdr[RELPLT].sh_size) / sizeof(Elf32_Rel));
	g_shdr[PLT].sh_addralign = 4;
	
	g_shdr[TEXT].sh_name = strstr(g_str, ".text") - g_str;
	g_shdr[TEXT].sh_type = SHT_PROGBITS;
	g_shdr[TEXT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	g_shdr[TEXT].sh_addr = g_shdr[PLT].sh_addr + g_shdr[PLT].sh_size;
	g_shdr[TEXT].sh_offset = g_shdr[TEXT].sh_addr;
	g_shdr[TEXT].sh_size = g_shdr[ARMEXIDX].sh_addr - g_shdr[TEXT].sh_addr;
	
	g_shdr[STRTAB].sh_name = strstr(g_str, ".shstrtab") - g_str;
	g_shdr[STRTAB].sh_type = SHT_STRTAB;
	g_shdr[STRTAB].sh_flags = SHT_NULL;
	g_shdr[STRTAB].sh_addr = 0;	//写文件的时候修正
	g_shdr[STRTAB].sh_size = strlen(g_str) + 1;
	g_shdr[STRTAB].sh_addralign = 1;
}

int main(int argc, char const *argv[])
{
	FILE *fr = NULL, *fw = NULL;
	char *buffer = NULL;
	Elf32_Ehdr ehdr = {0};
	Elf32_Phdr *pphdr = NULL;
	
	if (argc < 2) {
		printf("<so_path>\n");
		return -1;
	}
	
	fr = fopen(argv[1],"rb");
	//unsigned base = (unsigned)strtol(argv[2], 0, 16);
	
	if(fr == NULL) {
		printf("Open failed: \n");
		goto error;
	}
	
	int flen = get_file_len(fr);
	
	buffer = (char*)malloc(flen);
	if (buffer == NULL) {
		printf("Malloc error\n");
		goto error;
	}
	
	unsigned long result = fread (buffer,1,flen,fr);
	if (result != flen) {
		printf("Reading error\n");
		goto error;
	}
	
	fw = fopen("fix.so","wb");
	if(fw == NULL) {
		printf("Open failed: fix.so\n");
		goto error;
	}
	
	get_elf_header(&ehdr, buffer);
	//ehdr.e_entry = base;
	
	pphdr = (Elf32_Phdr*)(buffer + ehdr.e_phoff);
	
	regen_section_header(pphdr, &ehdr, buffer);
	
	size_t len_gstr = strlen(g_str);
	ehdr.e_shnum = SHDRS;
	//倒数第一个为段名字符串段
	ehdr.e_shstrndx = SHDRS - 1;
	
	//段表头紧接住段表最后一个成员--字符串段之后
	ehdr.e_shoff = flen + len_gstr + 1;
	
 	size_t szEhdr = sizeof(Elf32_Ehdr);
	//Elf头
	fwrite(&ehdr, szEhdr, 1, fw);
	//除了Elf头之外的原文件内容
	fwrite(buffer+szEhdr, flen-szEhdr, 1, fw);
	//补上段名字符串段
	g_shdr[STRTAB].sh_offset = flen;
	
	fwrite(g_strtabcontent, len_gstr + 1, 1, fw);
	//补上段表头
	fwrite(&g_shdr, sizeof(g_shdr), 1, fw);
	
error:
	if(fw != NULL)
		fclose(fw);
	if(fr != NULL)
		fclose(fr);
	free(buffer);
	return 0;
}
