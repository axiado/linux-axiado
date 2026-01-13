// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include "adapter_init.h" // Adapter_*
#include "adapter_global_init.h" // adapter_global_init/UnInit()
#include "adapter_global_cs_init.h" // adapter_global_cs_init/UnInit()
#include "adapter_global_drbg_init.h" // adapter_global_drbg_init/UnInit()
#include "log.h"

/**
 * @brief driver_197_init - init driver_197 APIS
 *
 */
int driver_197_init(void)
{
	if (!adapter_global_init()) {
		adapter_uninit();
		return -1;
	}

	if (!adapter_global_cs_init()) {
		adapter_global_uninit();
		adapter_uninit();
		return -1;
	}

	if (!adapter_global_drbg_init()) {
		adapter_global_cs_uninit();
		adapter_global_uninit();
		adapter_uninit();
		return -1;
	}
	LOG_DEBG("Driver 197 init done\n");
	return 0;
}

/**
 * @brief driver_197_unint - uninit driver 197 apis
 *
 */
void driver_197_uninit(void)
{
	adapter_global_drbg_uninit();
	adapter_global_cs_uninit();
	adapter_global_uninit();

	LOG_DEBG("\n\t driver197_exit done\n");
}
