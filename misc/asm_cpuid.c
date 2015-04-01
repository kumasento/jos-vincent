
#include <stdio.h>
#include <stdint.h>

static __inline void
cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp)
{
	uint32_t eax, ebx, ecx, edx;
	asm volatile("cpuid"
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		: "a" (info));
	if (eaxp)
		*eaxp = eax;
	if (ebxp)
		*ebxp = ebx;
	if (ecxp)
		*ecxp = ecx;
	if (edxp)
		*edxp = edx;
}

void print_vendor(uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    int i;
    printf("Vendor Name: ");
    for (i = 0; i < 32; i+=8) printf("%c", ebx>>i);
    for (i = 0; i < 32; i+=8) printf("%c", edx>>i);
    for (i = 0; i < 32; i+=8) printf("%c", ecx>>i);
    printf("\n");
}

int main()
{
    uint32_t info;
    uint32_t eax, ebx, ecx, edx;

    info = 0;
    cpuid(info, &eax, &ebx, &ecx, &edx);

    printf("Highest function: 0x%x\n", eax);
    print_vendor(ebx, ecx, edx);

    return 0;
}
