// dynarec.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <Windows.h>


#define VAL2CHARS(x) (char)(x & 0xFF), (char)((x >> 8) & 0xFF), (char)((x >> 16) & 0xFF), (char)((x >> 24) & 0xFF)

#define MOV_EAX_VAL(x) '\xb8', VAL2CHARS(x)
#define MOV_ECX_VAL(x) '\xb9', VAL2CHARS(x)
#define MOV_EDX_VAL(x) '\xba', VAL2CHARS(x)
#define MOV_EBX_VAL(x) '\xbb', VAL2CHARS(x)

#define ADD_EAX_VAL(x) '\x05', VAL2CHARS(x)
#define ADD_EBX_VAL(x) '\x81', '\xc3', VAL2CHARS(x)


constexpr uint8_t INC_EAX[] = { '\xff', '\xc0' };
constexpr uint8_t RET = '\xC3';
constexpr uint8_t NOP = '\x90';


const char* bfCode = "+++";

//const uint8_t exec_code[] = { MOV_EAX_VAL(0), INC_EAX, RET };

uint8_t stack[32];

typedef int32_t(*fn_ret_int)();

void print() {
	int x = 55;
	x--;
	printf("%d", x);
}

int main() {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	DWORD page_size = system_info.dwPageSize;
	
	uint8_t* buffer = (uint8_t*)VirtualAlloc(nullptr, page_size, MEM_COMMIT, PAGE_READWRITE);

	if (buffer == nullptr)
		return 1;

	DWORD dummy;
	VirtualProtect(buffer, 64, PAGE_EXECUTE_READWRITE, &dummy);
	
	//NOP and RET so we don't crash if nothing is done
	memset(buffer, NOP, page_size);
	memset(buffer + page_size - 1, RET, 1);

	uint8_t *s = (uint8_t*)(print);

	//memcpy_s(buffer, page_size, exec_code, sizeof(exec_code));

	uint8_t* pos = buffer;
	*pos++ = '\xb8';
	*pos++ = '\x00';
	*pos++ = '\x00';
	*pos++ = '\x00';
	*pos++ = '\x00';

	for (int i = 0; i < strlen(bfCode); i++) {
		switch (bfCode[i]) {
		case '+':
			//INC EAX
			*pos++ = '\xff';
			*pos++ = '\xc0';
			break;
		}
	}


	fn_ret_int x = (fn_ret_int)buffer;
	std::cout << x() << std::endl;

	VirtualFree(buffer, 0, MEM_RELEASE);

	return 0;
}