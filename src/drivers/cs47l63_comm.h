/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _CS47L63_COMM_H_
#define _CS47L63_COMM_H_

#include "cs47l63.h"
#include <stdint.h>

/**
 * @brief Initialize the CS47L63 codec communication over SPI
 *
 * @param driver Pointer to CS47L63 driver
 * @return 0 on success, negative errno on failure.
 */
int cs47l63_comm_init(cs47l63_t* driver);

/**
 * @brief Write register configuration array to CS47L63
 *
 * @param conf 2D array of [address, value]
 * @param size Number of elements
 * @return 0 on success, negative errno on failure.
 */
int cs47l63_comm_reg_conf_write(const uint32_t conf[][2], uint32_t size);

#endif /* _CS47L63_COMM_H_ */