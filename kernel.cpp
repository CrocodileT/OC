extern "C" int kmain();
__declspec(naked) void startup() {
	__asm {
		call kmain;
	}
}

#define IDT_TYPE_INTR (0x0E) 
#define IDT_TYPE_TRAP (0x0F) 

// �������� ������ ����, ������������� ����������� �� 
#define GDT_CS          (0x8) 

#define PIC1_PORT (0x20) 
#define VIDEO_BUF_PTR  (0xb8000) 
#define IDT_TYPE_MY (0x21)
#define KEY_BUFFER_SIZE 4

#pragma pack(push, 1)

int color;
int bad_flag;
int leng_buf;
char buffer[41];
int str_pos;
int pos;

struct idt_entry {
	unsigned short base_lo;    // ������� ���� ������ �����������     
	unsigned short segm_sel;   // �������� �������� ����     
	unsigned char always0;     // ���� ���� ������ 0     
	unsigned char flags;       // ����� ���. �����: P, DPL, ���� - ��� ��������� - IDT_TYPE...     
	unsigned short base_hi;    // ������� ���� ������ ����������� 
};

// ���������, ����� ������� ���������� ��� �������� ������� lidt 
struct idt_ptr {
	unsigned short limit;
	unsigned int base;
};

#pragma pack(pop) 

struct idt_entry g_idt[256]; // �������� ������� IDT 
struct idt_ptr g_idtp;

__declspec(naked) void default_intr_handler() {
	__asm {   pusha  }

	__asm {
		popa
		iretd
	}
}

typedef void(*intr_handler)();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr) {
	unsigned int hndlr_addr = (unsigned int)hndlr;

	g_idt[num].base_lo = (unsigned short)(hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short)(hndlr_addr >> 16);
}

void intr_init() {
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);

	for (int i = 0; i < idt_count; i++) {
		intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr 
	}
}

void intr_start() {
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);

	g_idtp.base = (unsigned int)(&g_idt[0]);
	g_idtp.limit = (sizeof(struct idt_entry) * idt_count) - 1;

	__asm {
		lidt g_idtp
	}

	//__lidt(&g_idtp); 
}

void intr_enable() {
	__asm sti;
}
void intr_disable() {
	__asm cli;
}

__inline unsigned char inb(unsigned short port) {// ������ �� �����
	unsigned char data;
	__asm {
		push dx
		mov dx, port
		in al, dx
		mov data, al
		pop dx   }
	return data;
}

__inline void outb(unsigned short port, unsigned char data) { // ������ 
	__asm {
		push dx
		mov dx, port
		mov al, data
		out dx, al
		pop dx
	}
}

void out_str(int color, const char* ptr, unsigned int strnum, int pos) {
	unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
	video_buf += 80 * 2 * strnum + pos * 2;

	while (*ptr) {
		video_buf[0] = (unsigned char)*ptr; // ������ (���) 
		video_buf[1] = color; // ���� ������� � ���� 

		video_buf += 2;
		ptr++;
	}
}

// ������� ���� ���������� �������� ���������� ������. 
//�������� ��� �����������, �� ����� ���������� � ������ BIOS � � ����� ������ ����� ������ ���� �������� �� BIOS data area.  
#define CURSOR_PORT (0x3D4) 
#define VIDEO_WIDTH  (80) // ������ ���������� ������ 

// ������� ��������� ������ �� ������ strnum (0 � ����� �������) � ������� pos �� ���� ������ (0 � ����� ����� ���������). 
void cursor_moveto(unsigned int strnum, unsigned int pos) {
	unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
	outb(CURSOR_PORT, 0x0F);
	outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
	outb(CURSOR_PORT, 0x0E);
	outb(CURSOR_PORT + 1, (unsigned char)((new_pos >> 8) & 0xFF));
}

int cmp(char *str, char *check, int n) {
	for (int i = 0; i < n; ++i)
	{
		if (str[i] != check[i]) {
			return 0;
		}
	}
	return 1;
}

void erase_buf() { //������� ������ ����� 
	for (int i = 0; i < 41; ++i) {
		buffer[i] = '\0';
	}
	leng_buf = 0;
}

void clear() {  //������� ������
	for (int i = 0; i < 80; i++) {
		for (int j = 0; j < 25; j++) {
			out_str(color, " ", j, i);
		}
	}
	str_pos = 0;
	pos = 0;
}

void loop() {
	while (1) {
		__asm {
			hlt
		}
	}
}

int char_to_int(char c) {
	int res;
	if (c < 58) {
		res = c - '0';
	}
	else {
		res = c - 'a';
	}
	return res;
}

char int_to_char(int n) {
	char res;
	if (n < 10) {
		res = char(n + '0');
	}
	else {
		res = char(n + 'a' - 10);
	}
	return res;
}

//������� ���������� ����� � ��� 23:12:12
void convert_time(int N, char *res) {
	int range = 0;
	int n = 1;
	while (n < N) {
		n *= 10;
		range++;
	}

	if (N == 0) {
		res[0] = '0';
		res[1] = '0';
		res[2] = 0;
		return;
	}
	else if (N == 1) {
		res[0] = '0';
		res[1] = '1';
		res[2] = 0;
		return;
	}
	else if (range == 1) {
		res[0] = '0';
		res[1] = int_to_char(N);
		res[2] = 0;
		return;
	}
	else {
		res[range--] = 0;
		while (N != 0) {
			res[range--] = int_to_char(N % 10);
			N /= 10;
		}
	}
}

//������� ��������� ������
void on_key(unsigned char scan_code)
{
	int n = (int)scan_code;

	char res_symb[3];
	res_symb[1] = '\0';
	switch (n)
	{
	case 2:  res_symb[0] = '1'; break;
	case 3:  res_symb[0] = '2'; break;
	case 4:  res_symb[0] = '3'; break;
	case 5:  res_symb[0] = '4'; break;
	case 6:  res_symb[0] = '5'; break;
	case 7:  res_symb[0] = '6'; break;
	case 8:  res_symb[0] = '7';	break;
	case 9:  res_symb[0] = '8'; break;
	case 10:  res_symb[0] = '9'; break;
	case 11:  res_symb[0] = '0'; break;
	case 52:  res_symb[0] = '.'; break;
	case 53:  res_symb[0] = '/'; break;
	case 12:  res_symb[0] = '-'; break;
	case 16:  res_symb[0] = 'q'; break;
	case 17:  res_symb[0] = 'w'; break;
	case 18:  res_symb[0] = 'e'; break;
	case 19:  res_symb[0] = 'r'; break;
	case 20:  res_symb[0] = 't'; break;
	case 21:  res_symb[0] = 'y'; break;
	case 22:  res_symb[0] = 'u'; break;
	case 23:  res_symb[0] = 'i'; break;
	case 24:  res_symb[0] = 'o'; break;
	case 25:  res_symb[0] = 'p'; break;
	case 30:  res_symb[0] = 'a'; break;
	case 31:  res_symb[0] = 's'; break;
	case 32:  res_symb[0] = 'd'; break;
	case 33:  res_symb[0] = 'f'; break;
	case 34:  res_symb[0] = 'g'; break;
	case 35:  res_symb[0] = 'h'; break;
	case 36:  res_symb[0] = 'j'; break;
	case 37:  res_symb[0] = 'k'; break;
	case 38:  res_symb[0] = 'l'; break;
	case 44:  res_symb[0] = 'z'; break;
	case 45:  res_symb[0] = 'x'; break;
	case 46:  res_symb[0] = 'c'; break;
	case 47:  res_symb[0] = 'v'; break;
	case 48:  res_symb[0] = 'b'; break;
	case 49:  res_symb[0] = 'n'; break;
	case 50:  res_symb[0] = 'm'; break;
	case 57:  res_symb[0] = ' '; break;
	case 28:  res_symb[0] = 'e'; res_symb[1] = 'n'; res_symb[2] = '\0'; break;
	case 14:  res_symb[0] = 'b'; res_symb[1] = 'c'; res_symb[2] = '\0'; break;
	default:
		bad_flag = 1;
		res_symb[0] = '\0';
	}

	if (n != 28 && n != 14 && n != 42 && leng_buf < 40)
	{
		res_symb[1] = '\0';
		if (bad_flag == 0)
		{
			out_str(color, res_symb, str_pos, pos);
			buffer[leng_buf++] = res_symb[0];
			cursor_moveto(str_pos, ++pos);
			return;
		}

	}
	else if (n == 14 && leng_buf > 0)//��������
	{
		pos--;
		cursor_moveto(str_pos, pos);
		out_str(color, " ", str_pos, pos);
		buffer[leng_buf] = '\0';
		leng_buf--;
		return;
	}
	else if (n == 28)
	{
		if (cmp(buffer, "clear", 5) == 1 && leng_buf == 5) {//�������

			clear();
			cursor_moveto(str_pos, pos);
			erase_buf();
		}
		else if (cmp(buffer, "info", 4) == 1 && leng_buf == 4) {
			str_pos++;

			out_str(color, "InfoOS: v.1 Developer: Crocodile, 23656/5, SpbPU, 2019", str_pos++, pos);
			out_str(color, "Compilers: bootloader: FASM| kernel: MS C", str_pos++, pos);
			erase_buf();
			pos = 0;
			cursor_moveto(str_pos, pos);

		}
		else if (cmp(buffer, "nsconv", 6) == 1 && leng_buf < 40) {//��������� ����� �� ����� ������� ���������� � ������
			str_pos++;

			int index = 7;
			int leng_bas = 0;
			int leng_new_bas = 0;
			int leng_num = 0;
			int basis[2];
			int new_basis[2];
			int bas = 0;
			int new_bas = 0;
			int num[30];
			int N = 0;

			while (buffer[index] != ' ') {
				num[leng_num++] = char_to_int(buffer[index++]);
			}

			index++;
			while (buffer[index] != ' ') {
				basis[leng_bas++] = char_to_int(buffer[index++]);
			}
			int ten = 1;
			for (int i = leng_bas - 1; i >= 0; i--) {
				bas += basis[i] * ten;
				ten *= 10;
			}
			if (bas > 36) {
				out_str(color, "error: basis", str_pos++, pos);
				erase_buf();
				pos = 0;
				cursor_moveto(str_pos, pos);
				return;
			}

			ten = 1;
			for (int i = leng_num - 1; i >= 0; i--) {
				N += num[i] * ten;
				ten *= bas;
			}


			index++;
			while (index < leng_buf) {
				new_basis[leng_new_bas++] = char_to_int(buffer[index++]);
			}
			ten = 1;
			for (int i = leng_new_bas - 1; i >= 0; i--) {
				new_bas += new_basis[i] * ten;
				ten *= 10;
			}
			if (new_bas > 36) {
				out_str(color, "error: new basis", str_pos++, pos);
				erase_buf();
				pos = 0;
				cursor_moveto(str_pos, pos);
				return;
			}


			int new_res[100];
			char res[100];
			int range = 0;
			if (N == 0) {
				res[0] = 0;
				range = 1;
			}
			while (N != 0) {
				new_res[range++] = N % new_bas;
				N /= new_bas;
			}

			for (int i = 1; i <= range; i++) {
				res[range - i] = int_to_char(new_res[i - 1]);
			}
			res[range] = '\0';

			out_str(color, res, str_pos++, pos);

			erase_buf();
			pos = 0;
			cursor_moveto(str_pos, pos);

		}
		else if (cmp(buffer, "shutdown", 8) == 1 && leng_buf == 8) {//����������
			clear();
			out_str(color, "You kill me!!!", str_pos, pos);
			loop();
			return;
		}

		else if (cmp(buffer, "posixtime", 9) == 1 && leng_buf < 40) {//����� ������� ������
			str_pos++;                                               //�� ���� ���������� ����� � ������� ��������� � 1970 ����
			int index = 10;
			int num[30];
			int leng_num = 0;
			long int N = 0;

			while (index < leng_buf) {
				num[leng_num++] = char_to_int(buffer[index++]);
			}

			int ten = 1;
			for (int i = leng_num - 1; i >= 0; i--) {
				N += num[i] * ten;
				ten *= 10;
			}

			int for_data = N / 3600 / 24;

			int year = for_data / 365 + 1970;
			int month = (for_data % 365) / 12 + 1;
			int day = ((for_data % 365) % 12) + 1;


			int hours = (N / 3600) % 24;
			int min = (N / 60) % 60;
			int sec = N % 60;
			char res_year[5];
			char res_month[3];
			char res_day[3];
			char res_hours[3];
			char res_min[3];
			char res_sec[3];
			convert_time(year, res_year);
			convert_time(month, res_month);
			convert_time(day, res_day);
			convert_time(hours, res_hours);
			convert_time(min, res_min);
			convert_time(sec, res_sec);
			char res_str[30];
			index = 0;
			for (int i = 0; i < 4; i++) {
				res_str[index++] = res_year[i];
			}
			res_str[index++] = '.';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_month[i];
			}
			res_str[index++] = '.';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_day[i];
			}
			res_str[index++] = ' ';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_hours[i];
			}
			res_str[index++] = ':';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_min[i];
			}
			res_str[index++] = ':';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_sec[i];
			}
			res_str[index] = 0;

			out_str(color, res_str, str_pos++, pos);
			pos = 0;
			erase_buf();
			cursor_moveto(str_pos, pos);
		}
		else if (cmp(buffer, "wintime", 7) == 1 && leng_buf < 40) {//�� ���� ���������� ����� � ������� ��������� � 1970 ���� � ������������
		    str_pos++;
			int index = 8;
			int num1[15];
			int num2[15];
			int leng_num = 0;
			long int N1 = 0;
			long int N2 = 0;
			while (index < leng_buf - 8) {
				num1[leng_num++] = char_to_int(buffer[index++]);
			}

			long int ten = 1;
			for (int i = leng_num - 1; i >= 0; i--) {
				N1 += num1[i] * ten;
				ten *= 10;
			}

			int for_data = N1 / 3600 / 24 * 10;

			int year = for_data / 365 + 1601;
			int month = (for_data % 365) / 12;
			int day = ((for_data % 365) % 12);

			leng_num = 0;
			while (index < leng_buf) {
				num2[leng_num++] = char_to_int(buffer[index++]);
			}
			ten = 1;
			for (int i = leng_num - 1; i >= 0; i--) {
				N2 += num2[i] * ten;
				ten *= 10;
			}

			int hours = (N2 / 3600) % 24;
			int min = (N2 / 60) % 60;
			int sec = N2 % 60;
			char res_year[5];
			char res_month[3];
			char res_day[3];
			char res_hours[3];
			char res_min[3];
			char res_sec[3];
			convert_time(year, res_year);
			convert_time(month, res_month);
			convert_time(day, res_day);
			convert_time(hours, res_hours);
			convert_time(min, res_min);
			convert_time(sec, res_sec);
			char res_str[100];
			index = 0;
			for (int i = 0; i < 4; i++) {
				res_str[index++] = res_year[i];
			}
			res_str[index++] = '.';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_month[i];
			}
			res_str[index++] = '.';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_day[i];
			}
			res_str[index++] = ' ';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_hours[i];
			}
			res_str[index++] = ':';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_min[i];
			}
			res_str[index++] = ':';
			for (int i = 0; i < 2; i++) {
				res_str[index++] = res_sec[i];
			}
			res_str[index] = 0;
			out_str(color, res_str, str_pos++, pos);
			pos = 0;
			erase_buf();
			cursor_moveto(str_pos, pos);
		}
		else
		{
			str_pos++;
			if (str_pos > 23) {
				clear();
			}
			out_str(color, "Unknown command", str_pos++, pos);
			pos = 0;
			erase_buf();
			cursor_moveto(str_pos, pos);
			return;
		}
	}
	else if (n == 42)
	{
		res_symb;
		return;
	}
}

void keyb_process_keys() {  // �������� ��� ����� PS/2 ���������� �� ���� (������� ��� ������������)  
	if (inb(0x64) & 0x01) {
		unsigned char scan_code;
		unsigned char state;

		scan_code = inb(0x60); // ���������� ������� � PS/2 ���������� 

		if (scan_code < 128)  // ����-���� ���� 128 - ��� ���������� �������   
			on_key(scan_code);
	}
}

__declspec(naked) void keyb_handler() {
	__asm pusha;

	// ��������� ����������� ������  
	keyb_process_keys();

	// �������� ����������� 8259 ����������� � ���, ��� ���������� ����������.
	//���� �� ���������� �����������, �� ���������� �� ����� �������� ����� �������� � ����������� �� ��� ���, ���� ��� �� �������� ��� ���������� ����������.   
	outb(PIC1_PORT, 0x20);

	__asm {
		popa
		iretd
	}
}

void keyb_init() {
	// ����������� ����������� ����������  
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
	// segm_sel=0x8, P=1, DPL=0, Type=Intr  
 // ���������� ������ ���������� ���������� �� ����������� 8259  
	outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - ��� ����������, 0x02 - ��� IRQ1 (����������).  
// ��������� ����� ������ ����������, ��� ���� ����������� � 0 
}

extern "C" int kmain() {
	const char* hello = "Welcome to OS!";
	pos = 0;
	bad_flag = 0;
	__asm {
		mov color, ecx;
	}
	out_str(color, hello, str_pos++, pos);
	str_pos++;

	// ����������� ����  
	cursor_moveto(str_pos, pos);
	intr_init();
	keyb_init();
	intr_start();
	intr_enable();
	loop();
	return 0;
}