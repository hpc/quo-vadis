#include "rocm_smi/rocm_smi.h"

int main(void)
{
    rsmi_status_t rsmi_rc = rsmi_init(0);
    return 0;
}
